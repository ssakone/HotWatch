package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/fsnotify/fsnotify"
	"github.com/gorilla/websocket"
)

type Server struct {
	watchDir    string
	port        int
	watcher     *fsnotify.Watcher
	clients     map[*websocket.Conn]bool
	clientsLock sync.Mutex
	upgrader    websocket.Upgrader
}

type FileChangeEvent struct {
	Type    string `json:"type"`
	Path    string `json:"path"`
	Message string `json:"message,omitempty"`
}

func NewServer(watchDir string, port int) (*Server, error) {
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		return nil, fmt.Errorf("failed to create watcher: %v", err)
	}

	return &Server{
		watchDir: watchDir,
		port:     port,
		watcher:  watcher,
		clients:  make(map[*websocket.Conn]bool),
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool {
				return true
			},
		},
	}, nil
}

func (s *Server) watchFiles() error {
	err := filepath.Walk(s.watchDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() {
			ext := strings.ToLower(filepath.Ext(path))
			if ext == ".qml" || ext == ".js" || info.Name() == "qmldir" {
				dir := filepath.Dir(path)
				if err := s.watcher.Add(dir); err != nil {
					log.Printf("Error watching directory %s: %v", dir, err)
				}
			}
		}
		return nil
	})
	if err != nil {
		return fmt.Errorf("failed to walk directory: %v", err)
	}
	return nil
}

func (s *Server) handleFileChanges() {
	for {
		select {
		case event, ok := <-s.watcher.Events:
			if !ok {
				return
			}
			if event.Has(fsnotify.Write) || event.Has(fsnotify.Create) {
				ext := strings.ToLower(filepath.Ext(event.Name))
				if ext == ".qml" || ext == ".js" || filepath.Base(event.Name) == "qmldir" {
					log.Printf("File changed: %s", event.Name)
					s.notifyClients(event.Name)
				}
			}
		case err, ok := <-s.watcher.Errors:
			if !ok {
				return
			}
			log.Printf("Watcher error: %v", err)
		}
	}
}

func (s *Server) notifyClients(path string) {
	event := FileChangeEvent{
		Type: "fileChanged",
		Path: path,
	}

	jsonMsg, err := json.Marshal(event)
	if err != nil {
		log.Printf("Error marshaling event: %v", err)
		return
	}

	log.Printf("Sending change notification for: %s", path)
	log.Printf("Message content: %s", string(jsonMsg))

	s.clientsLock.Lock()
	clientCount := len(s.clients)
	log.Printf("Number of connected clients: %d", clientCount)

	for client := range s.clients {
		err := client.WriteMessage(websocket.TextMessage, jsonMsg)
		if err != nil {
			log.Printf("Error sending to client: %v", err)
			client.Close()
			delete(s.clients, client)
		} else {
			log.Printf("Successfully sent notification to client")
		}
	}
	s.clientsLock.Unlock()
}

func (s *Server) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	log.Printf("WebSocket connection attempt from %s", r.RemoteAddr)

	// Accepter toutes les origines pour le développement
	s.upgrader.CheckOrigin = func(r *http.Request) bool {
		log.Printf("Checking origin: %s", r.Header.Get("Origin"))
		return true
	}

	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket upgrade failed: %v", err)
		return
	}

	log.Printf("New WebSocket client connected from %s", r.RemoteAddr)

	// Envoyer un message de test pour vérifier la connexion
	testMsg := FileChangeEvent{
		Type: "connected",
		Path: "test",
	}
	jsonMsg, _ := json.Marshal(testMsg)
	if err := conn.WriteMessage(websocket.TextMessage, jsonMsg); err != nil {
		log.Printf("Error sending test message: %v", err)
		conn.Close()
		return
	}

	s.clientsLock.Lock()
	s.clients[conn] = true
	clientCount := len(s.clients)
	s.clientsLock.Unlock()
	log.Printf("Total connected clients: %d", clientCount)

	// Garder la connexion active et écouter les messages
	go func() {
		for {
			messageType, message, err := conn.ReadMessage()
			if err != nil {
				log.Printf("WebSocket read error: %v", err)
				s.clientsLock.Lock()
				delete(s.clients, conn)
				s.clientsLock.Unlock()
				conn.Close()
				return
			}
			log.Printf("Received message type: %d", messageType)
			
			// Handle error messages from client
			if messageType == websocket.TextMessage {
				var event FileChangeEvent
				if err := json.Unmarshal(message, &event); err == nil {
					if event.Type == "error" {
						log.Printf("Client Error: %s", event.Message)
					}
				}
			}
		}
	}()
}

func (s *Server) handleFileRequest(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	if path == "/" {
		path = "/index.qml"
	}

	filePath := filepath.Join(s.watchDir, path)
	http.ServeFile(w, r, filePath)
}

func (s *Server) handleDiscovery(port int) {
	addr := net.UDPAddr{
		Port: 45454,
		IP:   net.IPv4(0, 0, 0, 0),
	}

	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		log.Printf("Discovery service error: %v", err)
		return
	}
	defer conn.Close()

	buffer := make([]byte, 1024)
	for {
		n, remoteAddr, err := conn.ReadFromUDP(buffer)
		if err != nil {
			log.Printf("Error reading UDP: %v", err)
			continue
		}

		if string(buffer[:n]) == "HotWatchDiscovery" {
			// Get local IP
			addrs, err := net.InterfaceAddrs()
			if err != nil {
				log.Printf("Error getting interface addresses: %v", err)
				continue
			}

			fmt.Printf("Received discovery request from %s\n", remoteAddr.String())

			for _, addr := range addrs {
				if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
					if ipv4 := ipnet.IP.To4(); ipv4 != nil {
						response := fmt.Sprintf("HotWatchServer:http://%s:%d", ipv4.String(), port)
						conn.WriteToUDP([]byte(response), remoteAddr)
						break
					}
				}
			}
		}
	}
}

func main() {
	watchDir := flag.String("dir", ".", "Directory to watch")
	port := flag.Int("port", 8080, "Port to listen on")
	flag.Parse()

	server, err := NewServer(*watchDir, *port)
	if err != nil {
		log.Fatal(err)
	}

	if err := server.watchFiles(); err != nil {
		log.Fatal(err)
	}

	go server.handleFileChanges()
	go server.handleDiscovery(*port)

	http.HandleFunc("/ws", server.handleWebSocket)
	http.HandleFunc("/", server.handleFileRequest)

	addr := fmt.Sprintf(":%d", *port)
	log.Printf("Starting server on %s", addr)
	log.Printf("WebSocket endpoint: ws://localhost:%d/ws", *port)
	log.Printf("Watching directory: %s", *watchDir)

	if err := http.ListenAndServe(addr, nil); err != nil {
		log.Fatal(err)
	}
}
