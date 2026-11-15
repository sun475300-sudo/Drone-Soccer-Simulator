// Drone Soccer Lab serves its embedded UI on loopback only. Videos never reach this server.
package main

import (
	"context"
	"crypto/rand"
	"embed"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)

//go:embed web/*
var assets embed.FS
var version = "0.1.0-preview"

func newHandler(prefix, host string, stop func()) http.Handler {
	root, _ := fs.Sub(assets, "web")
	files := http.StripPrefix(prefix, http.FileServer(http.FS(root)))
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Referrer-Policy", "no-referrer")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; media-src 'self' blob:; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
		if r.Host != host {
			http.Error(w, "Unexpected host", http.StatusForbidden)
			return
		}
		if !strings.HasPrefix(r.URL.Path, prefix) {
			http.NotFound(w, r)
			return
		}
		origin := r.Header.Get("Origin")
		if origin != "" && origin != "http://"+host {
			http.Error(w, "Unexpected origin", http.StatusForbidden)
			return
		}
		route := strings.TrimPrefix(r.URL.Path, prefix)
		if route == "quit" {
			if r.Method != "POST" {
				http.Error(w, "POST required", http.StatusMethodNotAllowed)
				return
			}
			if r.Header.Get("X-Lab-Request") != "shutdown" {
				http.Error(w, "Missing intent", http.StatusForbidden)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			fmt.Fprint(w, `{"stopped":true}`)
			go stop()
			return
		}
		if r.Method != "GET" && r.Method != "HEAD" {
			http.Error(w, "Read only", http.StatusMethodNotAllowed)
			return
		}
		if route == "health" {
			w.Header().Set("Content-Type", "application/json")
			_ = json.NewEncoder(w).Encode(map[string]string{"app": "Drone Soccer Lab", "version": version})
			return
		}
		// Only embedded assets are reachable, never arbitrary filesystem paths.
		files.ServeHTTP(w, r)
	})
}

func openBrowser(url string) error {
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("rundll32.exe", "url.dll,FileProtocolHandler", url)
	case "darwin":
		cmd = exec.Command("open", url)
	default:
		cmd = exec.Command("xdg-open", url)
	}
	if err := cmd.Start(); err != nil {
		return err
	}
	go func() { _ = cmd.Wait() }()
	return nil
}
func run() error {
	port := flag.Int("port", 0, "Loopback port; 0 selects a free port")
	noOpen := flag.Bool("no-open", false, "Do not open a browser")
	selfTest := flag.Bool("self-test", false, "Validate embedded files and exit")
	flag.Parse()
	if *selfTest {
		for _, p := range []string{"web/index.html", "web/app.js", "web/core.js", "web/style.css"} {
			b, e := assets.ReadFile(p)
			if e != nil || len(b) == 0 {
				return fmt.Errorf("embedded asset missing: %s", p)
			}
		}
		fmt.Println("SELF_TEST_OK " + version)
		return nil
	}
	if *port < 0 || *port > 65535 {
		return errors.New("port must be in 0..65535")
	}
	listener, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", *port))
	if err != nil {
		return err
	}
	defer listener.Close()
	key := make([]byte, 24)
	if _, err = rand.Read(key); err != nil {
		return err
	}
	prefix := "/" + hex.EncodeToString(key) + "/"
	host := listener.Addr().String()
	url := "http://" + host + prefix
	srv := &http.Server{ReadHeaderTimeout: 5 * time.Second, IdleTimeout: 60 * time.Second, MaxHeaderBytes: 8192}
	shutdownDone := make(chan struct{})
	var stopOnce sync.Once
	stop := func() {
		stopOnce.Do(func() {
			ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
			defer cancel()
			_ = srv.Shutdown(ctx)
			close(shutdownDone)
		})
	}
	srv.Handler = newHandler(prefix, host, stop)
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	defer signal.Stop(sig)
	go func() { <-sig; stop() }()
	// A current session URL is written locally so it can be reopened if browser launch fails.
	if dir, e := os.UserConfigDir(); e == nil {
		dir = filepath.Join(dir, "DroneSoccerLab")
		if os.MkdirAll(dir, 0700) == nil {
			_ = os.WriteFile(filepath.Join(dir, "session.txt"), []byte(url+"\n"), 0600)
		}
	}
	fmt.Println(url)
	if !*noOpen {
		if e := openBrowser(url); e != nil {
			log.Printf("Open this local address in Edge/Chrome: %s (%v)", url, e)
		}
	}
	err = srv.Serve(listener)
	if errors.Is(err, http.ErrServerClosed) {
		<-shutdownDone // Let in-flight responses flush before the process exits.
		return nil
	}
	return err
}
func main() {
	if err := run(); err != nil {
		log.Print(err)
		os.Exit(1)
	}
}
