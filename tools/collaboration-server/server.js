// Collaboration Server Prototype
// npm init -y && npm install ws
// node server.js

const WebSocket = require('ws');
const http = require('http');

const PORT = process.env.PORT || 8080;

// Create HTTP server for health checks
const server = http.createServer((req, res) => {
    if (req.url === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'ok', clients: clients.size }));
    } else {
        res.writeHead(404);
        res.end();
    }
});

const wss = new WebSocket.Server({ server });

// Session storage: projectId -> { clients, operations, locks }
const sessions = new Map();
const clients = new Map(); // ws -> { projectId, clientId, userId, userName, userColor }

wss.on('connection', (ws, req) => {
    console.log('[Server] New connection');

    ws.on('message', (data) => {
        try {
            const msg = JSON.parse(data.toString());
            handleClientMessage(ws, msg);
        } catch (err) {
            console.error('[Server] Failed to parse message:', err.message);
            ws.send(JSON.stringify({ type: 'error', message: 'Invalid JSON' }));
        }
    });

    ws.on('close', () => {
        handleClientDisconnect(ws);
    });

    ws.on('error', (err) => {
        console.error('[Server] WebSocket error:', err.message);
    });
});

function handleClientMessage(ws, msg) {
    const clientInfo = clients.get(ws);

    switch (msg.type) {
        case 'join':
            handleJoin(ws, msg);
            break;

        case 'operation':
            handleOperation(ws, msg);
            break;

        case 'lock_request':
            handleLockRequest(ws, msg);
            break;

        case 'unlock_request':
            handleUnlockRequest(ws, msg);
            break;

        case 'presence':
            handlePresence(ws, msg);
            break;

        default:
            console.warn('[Server] Unknown message type:', msg.type);
    }
}

function handleJoin(ws, msg) {
    const projectId = msg.projectId;
    const clientId = msg.clientId;
    const userId = msg.userId || clientId;
    const userName = msg.userName || 'Anonymous';
    const userColor = msg.userColor || '#888888';

    // Register client
    clients.set(ws, { projectId, clientId, userId, userName, userColor });

    // Get or create session
    let session = sessions.get(projectId);
    if (!session) {
        session = { clients: new Set(), operations: [], locks: new Map() };
        sessions.set(projectId, session);
    }
    session.clients.add(ws);

    // Send history to new client
    if (session.operations.length > 0) {
        ws.send(JSON.stringify({
            type: 'history',
            operations: session.operations
        }));
    }

    // Notify others
    broadcast(session, {
        type: 'user_joined',
        clientId,
        userId,
        userName,
        userColor
    }, ws);

    console.log(`[Server] User ${userName} joined project ${projectId}`);
}

function handleOperation(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    // Apply operation transformation if needed
    // For now, simple broadcast with version tracking
    const operation = {
        ...msg.operation,
        version: session.operations.length,
        clientId: clientInfo.clientId,
        timestamp: msg.timestamp || new Date().toISOString()
    };

    session.operations.push(operation);

    // Broadcast to all other clients
    broadcast(session, {
        type: 'operation',
        clientId: clientInfo.clientId,
        projectId: clientInfo.projectId,
        operation,
        version: operation.version
    }, ws);
}

function handleLockRequest(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    const layerId = msg.layerId;

    if (!session.locks.has(layerId)) {
        // Grant lock
        session.locks.set(layerId, {
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName,
            acquiredAt: new Date().toISOString()
        });

        ws.send(JSON.stringify({
            type: 'lock_granted',
            layerId,
            clientId: clientInfo.clientId
        }));

        // Notify others
        broadcast(session, {
            type: 'lock_updated',
            layerId,
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName
        }, ws);

        console.log(`[Server] Lock granted: ${layerId} to ${clientInfo.userName}`);
    } else {
        const existingLock = session.locks.get(layerId);
        ws.send(JSON.stringify({
            type: 'lock_denied',
            layerId,
            reason: `Locked by ${existingLock.userName}`
        }));
    }
}

function handleUnlockRequest(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    const layerId = msg.layerId;
    const lock = session.locks.get(layerId);

    if (lock && lock.clientId === clientInfo.clientId) {
        session.locks.delete(layerId);

        ws.send(JSON.stringify({
            type: 'lock_released',
            layerId
        }));

        // Notify others
        broadcast(session, {
            type: 'lock_released',
            layerId,
            clientId: clientInfo.clientId
        }, ws);

        console.log(`[Server] Lock released: ${layerId} by ${clientInfo.userName}`);
    }
}

function handlePresence(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    // Broadcast presence to others
    broadcast(session, {
        type: 'presence',
        clientId: clientInfo.clientId,
        userId: clientInfo.userId,
        userName: clientInfo.userName,
        userColor: clientInfo.userColor,
        presence: msg.presence
    }, ws);
}

function handleClientDisconnect(ws) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (session) {
        session.clients.delete(ws);

        // Release all locks held by this client
        for (const [layerId, lock] of session.locks) {
            if (lock.clientId === clientInfo.clientId) {
                session.locks.delete(layerId);
                broadcast(session, {
                    type: 'lock_released',
                    layerId,
                    clientId: clientInfo.clientId,
                    reason: 'User disconnected'
                });
            }
        }

        // Notify others
        broadcast(session, {
            type: 'user_left',
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName
        });

        // Clean up empty sessions
        if (session.clients.size === 0) {
            sessions.delete(clientInfo.projectId);
        }

        console.log(`[Server] User ${clientInfo.userName} left project ${clientInfo.projectId}`);
    }

    clients.delete(ws);
}

function broadcast(session, msg, excludeWs = null) {
    const data = JSON.stringify(msg);
    for (const client of session.clients) {
        if (client !== excludeWs && client.readyState === WebSocket.OPEN) {
            client.send(data);
        }
    }
}

server.listen(PORT, () => {
    console.log(`[Server] Collaboration server started on port ${PORT}`);
    console.log(`[Server] Health check: http://localhost:${PORT}/health`);
});
