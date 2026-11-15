package main

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestAssets(t *testing.T) {
	h := newHandler("/secret/", "127.0.0.1:12345", func() {})
	for _, path := range []string{"", "index.html", "app.js", "core.js", "style.css", "health"} {
		r := httptest.NewRequest("GET", "http://127.0.0.1:12345/secret/"+path, nil)
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		if w.Code != 200 && w.Code != 301 {
			t.Fatalf("%s: %d", path, w.Code)
		}
		if w.Header().Get("Content-Security-Policy") == "" {
			t.Fatal("missing CSP")
		}
	}
}
func TestAccessGuards(t *testing.T) {
	h := newHandler("/secret/", "127.0.0.1:12345", func() {})
	cases := []struct {
		method, url, origin string
		want                int
	}{{"GET", "http://evil.example/secret/", "", 403}, {"GET", "http://127.0.0.1:12345/", "", 404}, {"POST", "http://127.0.0.1:12345/secret/", "", 405}, {"GET", "http://127.0.0.1:12345/secret/quit", "", 405}, {"POST", "http://127.0.0.1:12345/secret/quit", "", 403}, {"GET", "http://127.0.0.1:12345/secret/", "http://evil.example", 403}, {"GET", "http://127.0.0.1:12345/secret/no-file", "", 404}}
	for _, c := range cases {
		r := httptest.NewRequest(c.method, c.url, nil)
		r.Header.Set("Origin", c.origin)
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		if w.Code != c.want {
			t.Fatalf("%s got %d want %d", c.url, w.Code, c.want)
		}
	}
}
func TestQuit(t *testing.T) {
	done := make(chan bool, 1)
	h := newHandler("/secret/", "127.0.0.1:12345", func() { done <- true })
	r := httptest.NewRequest(http.MethodPost, "http://127.0.0.1:12345/secret/quit", strings.NewReader(""))
	r.Header.Set("Origin", "http://127.0.0.1:12345")
	r.Header.Set("X-Lab-Request", "shutdown")
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	if w.Code != 200 {
		t.Fatal(w.Code)
	}
	<-done
}
