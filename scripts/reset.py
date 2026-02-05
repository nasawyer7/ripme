from flask import Flask, request
import subprocess

app = Flask(__name__)

# Config
TARGET_VMID = "107"
AUTH_TOKEN = "MySecretKey123"

@app.route('/hard-reset', methods=['POST'])
def reset_vm():
    # Check if the requester sent the right secret key
    token = request.headers.get('Authorization')
    if token != AUTH_TOKEN:
        return "Unauthorized", 401

    try:
        
        subprocess.run(["qm", "reset", TARGET_VMID], check=True)
        return f"VM {TARGET_VMID} Reset Signal Sent", 200
    except Exception as e:
        return str(e), 500

if __name__ == '__main__':
    # Binding to 0.0.0.0 lets it hear requests from your web server
    app.run(host='0.0.0.0', port=5000)
