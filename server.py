#!/usr/bin/env python3
import http.server
import json
import subprocess
import tempfile
import os
import re
from urllib.parse import parse_qs

class AnalyzerHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # suppress default logging

    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            with open(os.path.join(os.path.dirname(__file__), 'index.html'), 'rb') as f:
                self.wfile.write(f.read())
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if self.path == '/analyze':
            content_length = int(self.headers['Content-Length'])
            body = self.rfile.read(content_length)

            # Parse multipart form
            content_type = self.headers['Content-Type']
            boundary = content_type.split('boundary=')[1].encode()

            parts = body.split(b'--' + boundary)
            bc_data = None
            threshold = '4096'

            for part in parts:
                if b'name="bcfile"' in part:
                    bc_data = part.split(b'\r\n\r\n', 1)[1].rstrip(b'\r\n--')
                elif b'name="threshold"' in part:
                    threshold = part.split(b'\r\n\r\n', 1)[1].strip().decode()

            if not bc_data:
                self.send_response(400)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': 'No file uploaded'}).encode())
                return

            # Write bc file to temp
            with tempfile.NamedTemporaryFile(suffix='.bc', delete=False) as f:
                f.write(bc_data)
                tmp_path = f.name

            try:
                analyzer = os.path.expanduser('~/stack-analyzer/build/stack-analyzer')
                result = subprocess.run(
                    [analyzer, tmp_path, f'--threshold={threshold}'],
                    capture_output=True, text=True, timeout=30
                )
                output = result.stderr + result.stdout
                parsed = parse_output(output, threshold)

                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(parsed).encode())
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode())
            finally:
                os.unlink(tmp_path)

def parse_output(output, threshold):
    result = {
        'functions': [],
        'chains': [],
        'overflow_risks': [],
        'recursions': [],
        'threshold': int(threshold)
    }

    lines = output.split('\n')
    section = None

    for line in lines:
        line = line.strip()
        if not line:
            continue

        if 'Per-Function Stack Frame Sizes' in line:
            section = 'functions'
        elif 'Top Call Chains' in line:
            section = 'chains'
        elif line.startswith('[OVERFLOW RISK]'):
            m = re.match(r'\[OVERFLOW RISK\] (\S+) worst-case=(\d+) bytes \(threshold=(\d+)\)', line)
            if m:
                result['overflow_risks'].append({
                    'function': m.group(1),
                    'depth': int(m.group(2)),
                    'threshold': int(m.group(3))
                })
        elif line.startswith('[RECURSION]'):
            result['recursions'].append(line.replace('[RECURSION]', '').strip())
        elif section == 'functions' and line.startswith(':') or (section == 'functions' and ':' in line and 'bytes' in line):
            m = re.match(r'(\S+):\s+(\d+) bytes', line)
            if m:
                result['functions'].append({'name': m.group(1), 'size': int(m.group(2))})
        elif section == 'chains' and re.match(r'#\d+', line):
            m = re.match(r'#(\d+)\s+(\d+) bytes:\s+(.+)', line)
            if m:
                chain_str = m.group(3)
                steps = [s.strip() for s in chain_str.split('->') if s.strip()]
                parsed_steps = []
                for step in steps:
                    sm = re.match(r'(.+)\((\d+)B\)', step)
                    if sm:
                        parsed_steps.append({'name': sm.group(1), 'size': int(sm.group(2))})
                result['chains'].append({
                    'rank': int(m.group(1)),
                    'depth': int(m.group(2)),
                    'path': chain_str,
                    'steps': parsed_steps
                })

    result['functions'].sort(key=lambda x: x['size'], reverse=True)
    return result

if __name__ == '__main__':
    port = 8765
    server = http.server.HTTPServer(('0.0.0.0', port), AnalyzerHandler)
    print(f'Stack Analyzer server running at http://localhost:{port}')
    server.serve_forever()
