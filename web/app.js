class ChatClient {
    constructor() {
        this.ws = null;
        this.username = null;
        this.currentRoom = 'general';
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
    }

    connect() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/`;
        
        console.log('Connecting to:', wsUrl);
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.reconnectAttempts = 0;
        };
        
        this.ws.onmessage = (event) => {
            try {
                const message = JSON.parse(event.data);
                this.handleMessage(message);
            } catch (error) {
                console.error('Failed to parse message:', error);
            }
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.showError('Connection error. Please try again.');
        };
        
        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            
            if (this.username && this.reconnectAttempts < this.maxReconnectAttempts) {
                this.reconnectAttempts++;
                console.log(`Reconnecting... Attempt ${this.reconnectAttempts}`);
                setTimeout(() => this.connect(), 2000);
            } else if (this.username) {
                this.showError('Connection lost. Please refresh the page.');
            }
        };
    }

    login(username, password) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            this.showError('Not connected to server. Please wait...');
            return;
        }
        
        const authMessage = {
            type: 'auth',
            username: username,
            password: password
        };
        
        this.ws.send(JSON.stringify(authMessage));
    }

    sendMessage(content, room) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            this.showError('Not connected to server.');
            return;
        }
        
        const message = {
            type: 'message',
            sender: this.username,
            room: room || this.currentRoom,
            content: content,
            timestamp: Math.floor(Date.now() / 1000)
        };
        
        this.ws.send(JSON.stringify(message));
    }

    handleMessage(message) {
        switch(message.type) {
            case 'auth_success':
                this.username = message.username || this.username;
                this.showChatScreen();
                break;
                
            case 'auth_failure':
                this.showError('Authentication failed. Please check your credentials.');
                break;
                
            case 'message':
                this.displayMessage(message);
                break;
                
            case 'history':
                if (message.messages && Array.isArray(message.messages)) {
                    message.messages.forEach(msg => this.displayMessage(msg));
                }
                break;
                
            case 'error':
                this.showError(message.content || 'An error occurred.');
                break;
                
            default:
                console.log('Unknown message type:', message.type);
        }
    }

    displayMessage(message) {
        const messagesDiv = document.getElementById('chat-messages');
        const messageEl = document.createElement('div');
        messageEl.className = 'message';
        
        const timestamp = message.timestamp ? 
            new Date(message.timestamp * 1000).toLocaleTimeString() : 
            new Date().toLocaleTimeString();
        
        messageEl.innerHTML = `
            <div class="message-header">
                <span class="sender">${this.escapeHtml(message.sender)}</span>
                <span class="timestamp">${timestamp}</span>
            </div>
            <div class="content">${this.escapeHtml(message.content)}</div>
        `;
        
        messagesDiv.appendChild(messageEl);
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
    }

    displayHistory(messages) {
        messages.forEach(msg => this.displayMessage(msg));
    }

    showChatScreen() {
        document.getElementById('login-screen').style.display = 'none';
        document.getElementById('chat-screen').style.display = 'flex';
        document.getElementById('current-user').textContent = this.username;
        document.getElementById('current-room').textContent = this.currentRoom;
        document.getElementById('message-input').focus();
    }

    showLoginScreen() {
        document.getElementById('chat-screen').style.display = 'none';
        document.getElementById('login-screen').style.display = 'flex';
        document.getElementById('chat-messages').innerHTML = '';
        this.username = null;
    }

    showError(message) {
        const errorDiv = document.getElementById('login-error');
        if (errorDiv) {
            errorDiv.textContent = message;
            setTimeout(() => {
                errorDiv.textContent = '';
            }, 5000);
        }
    }

    logout() {
        if (this.ws) {
            this.ws.close();
        }
        this.showLoginScreen();
        this.connect();
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize client
const client = new ChatClient();

// Wait for DOM to be ready
document.addEventListener('DOMContentLoaded', () => {
    // Connect to WebSocket
    client.connect();
    
    // Login form handler
    const loginForm = document.getElementById('login-form');
    if (loginForm) {
        loginForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value;
            
            if (username && password) {
                client.login(username, password);
            }
        });
    }
    
    // Message form handler
    const messageForm = document.getElementById('message-form');
    if (messageForm) {
        messageForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const input = document.getElementById('message-input');
            const content = input.value.trim();
            
            if (content) {
                const room = document.getElementById('current-room').textContent || 'general';
                client.sendMessage(content, room);
                input.value = '';
            }
        });
    }
    
    // Logout button handler
    const logoutBtn = document.getElementById('logout-btn');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', () => {
            client.logout();
        });
    }
});
