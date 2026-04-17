const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static(path.join(__dirname, 'public')));

const COLS = 10;
const ROWS = 20;
const PIECES = ['I', 'O', 'T', 'S', 'Z', 'J', 'L'];

const SHAPES = {
    I: [[1,1,1,1]],
    O: [[1,1],[1,1]],
    T: [[0,1,0],[1,1,1]],
    S: [[0,1,1],[1,1,0]],
    Z: [[1,1,0],[0,1,1]],
    J: [[1,0,0],[1,1,1]],
    L: [[0,0,1],[1,1,1]]
};

const COLORS = {
    I: '#00f5ff',
    O: '#ffdd00',
    T: '#b44fff',
    S: '#44ff44',
    Z: '#ff4444',
    J: '#4477ff',
    L: '#ff8844'
};

class Room {
    constructor(code) {
        this.code = code;
        this.players = new Map();
        this.gameStatus = 'waiting';
        this.gameLoop = null;
        this.tickRate = 500;
        this.lockDelayMs = 450;
        this.winner = null;
    }

    addPlayer(socketId, name) {
        const player = {
            id: socketId,
            name: name,
            board: this.createEmptyBoard(),
            currentPiece: null,
            nextPiece: null,
            holdPiece: null,
            holdUsed: false,
            lastDropAt: 0,
            lockStartedAt: null,
            score: 0,
            level: 1,
            lines: 0,
            status: 'waiting',
            ready: false,
            bag: [],
            garbagePending: [],
            koCount: 0
        };
        this.players.set(socketId, player);
        return player;
    }

    removePlayer(socketId) {
        this.players.delete(socketId);
        if (this.players.size === 0) {
            return true;
        }
        this.broadcastState();
        return false;
    }

    createEmptyBoard() {
        return Array(ROWS).fill(null).map(() => Array(COLS).fill(0));
    }

    generateRoomCode() {
        return Math.random().toString(36).substring(2, 8).toUpperCase();
    }

    getRandomPiece(player) {
        if (!player.bag || player.bag.length === 0) {
            player.bag = [...PIECES];
            for (let i = player.bag.length - 1; i > 0; i--) {
                const j = Math.floor(Math.random() * (i + 1));
                [player.bag[i], player.bag[j]] = [player.bag[j], player.bag[i]];
            }
        }
        return player.bag.pop();
    }

    rotateCW(shape) {
        const rows = shape.length;
        const cols = shape[0].length;
        const rotated = [];
        for (let c = 0; c < cols; c++) {
            const newRow = [];
            for (let r = rows - 1; r >= 0; r--) {
                newRow.push(shape[r][c]);
            }
            rotated.push(newRow);
        }
        return rotated;
    }

    rotateCCW(shape) {
        const rows = shape.length;
        const cols = shape[0].length;
        const rotated = [];
        for (let c = cols - 1; c >= 0; c--) {
            const newRow = [];
            for (let r = 0; r < rows; r++) {
                newRow.push(shape[r][c]);
            }
            rotated.push(newRow);
        }
        return rotated;
    }

    isValidPosition(board, shape, x, y) {
        for (let row = 0; row < shape.length; row++) {
            for (let col = 0; col < shape[row].length; col++) {
                if (shape[row][col]) {
                    const newX = x + col;
                    const newY = y + row;
                    if (newX < 0 || newX >= COLS || newY >= ROWS) {
                        return false;
                    }
                    if (newY >= 0 && board[newY][newX]) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    spawnPiece(player) {
        if (!player.nextPiece) {
            player.nextPiece = this.getRandomPiece(player);
        }
        player.currentPiece = {
            type: player.nextPiece,
            shape: SHAPES[player.nextPiece].map(row => [...row]),
            x: Math.floor((COLS - SHAPES[player.nextPiece][0].length) / 2),
            y: 0
        };
        player.nextPiece = this.getRandomPiece(player);
        player.holdUsed = false;
        player.lastDropAt = Date.now();
        player.lockStartedAt = null;
    }

    holdCurrentPiece(player) {
        if (player.status !== 'playing') return false;
        if (!player.currentPiece) return false;
        if (player.holdUsed) return false;

        const currentType = player.currentPiece.type;

        if (!player.holdPiece) {
            player.holdPiece = currentType;
            player.currentPiece = null;
            this.spawnPiece(player);
        } else {
            const swapType = player.holdPiece;
            player.holdPiece = currentType;
            player.currentPiece = {
                type: swapType,
                shape: SHAPES[swapType].map(row => [...row]),
                x: Math.floor((COLS - SHAPES[swapType][0].length) / 2),
                y: 0
            };
        }

        player.holdUsed = true;
        player.lockStartedAt = null;
        return true;
    }

    isPieceGrounded(player) {
        if (!player.currentPiece) return false;
        return !this.isValidPosition(
            player.board,
            player.currentPiece.shape,
            player.currentPiece.x,
            player.currentPiece.y + 1
        );
    }

    lockPiece(player) {
        const { board, currentPiece } = player;
        const { shape, x, y } = currentPiece;

        for (let row = 0; row < shape.length; row++) {
            for (let col = 0; col < shape[row].length; col++) {
                if (shape[row][col]) {
                    const boardY = y + row;
                    const boardX = x + col;
                    if (boardY >= 0 && boardY < ROWS && boardX >= 0 && boardX < COLS) {
                        board[boardY][boardX] = currentPiece.type;
                    }
                }
            }
        }

        player.score += 10;
        const result = this.clearLines(player);
        if (player.board[0].some(cell => cell !== 0)) {
            player.status = 'finished';
            player.currentPiece = null;
        }
        return result;
    }

    clearLines(player) {
        const { board } = player;
        let linesCleared = 0;
        let clearedRows = [];

        for (let row = ROWS - 1; row >= 0; row--) {
            if (board[row].every(cell => cell !== 0)) {
                clearedRows.push(row);
                linesCleared++;
            }
        }

        if (linesCleared > 0) {
            for (const row of clearedRows) {
                board.splice(row, 1);
                board.unshift(Array(COLS).fill(0));
            }

            player.lines += linesCleared;
            player.level = Math.floor(player.lines / 10) + 1;
            if (player.level > 15) player.level = 15;

            const points = [0, 100, 300, 500, 800][linesCleared] * player.level;
            player.score += points;

            const garbageToSend = [0, 0, 2, 4, 6][linesCleared];
            return { linesCleared, garbage: garbageToSend };
        }

        return { linesCleared: 0, garbage: 0 };
    }

    addGarbage(player, amount, attackerId = null) {
        for (let i = 0; i < amount; i++) {
            // If top row is already occupied, pushing up causes overflow/elimination.
            if (player.board[0].some(cell => cell !== 0)) {
                player.status = 'finished';
                player.currentPiece = null;
                if (attackerId) {
                    const attacker = this.players.get(attackerId);
                    if (attacker && attacker.id !== player.id) {
                        attacker.koCount += 1;
                    }
                }
                return;
            }
            player.board.shift();
            const garbageLine = Array(COLS).fill('GARBAGE');
            const holeIndex = Math.floor(Math.random() * COLS);
            garbageLine[holeIndex] = 0;
            if (Math.random() < 0.5) {
                const hole2 = (holeIndex + 1 + Math.floor(Math.random() * (COLS - 1))) % COLS;
                garbageLine[hole2] = 0;
            }
            player.board.push(garbageLine);
        }
    }

    movePiece(player, direction) {
        const { currentPiece, board } = player;
        if (!currentPiece) return false;

        let newX = currentPiece.x;
        let newY = currentPiece.y;

        switch (direction) {
            case 'left': newX--; break;
            case 'right': newX++; break;
            case 'down': newY++; break;
        }

        if (this.isValidPosition(board, currentPiece.shape, newX, newY)) {
            currentPiece.x = newX;
            currentPiece.y = newY;
            return true;
        }

        if (direction === 'down') {
            return false;
        }

        return false;
    }

    rotatePiece(player, direction) {
        const { currentPiece, board } = player;
        if (!currentPiece) return false;

        const newShape = direction === 'CW'
            ? this.rotateCW(currentPiece.shape)
            : this.rotateCCW(currentPiece.shape);

        const kicks = [
            [0, 0], [-1, 0], [1, 0], [0, -1], [-2, 0], [2, 0]
        ];

        for (const [dx, dy] of kicks) {
            if (this.isValidPosition(board, newShape, currentPiece.x + dx, currentPiece.y + dy)) {
                currentPiece.shape = newShape;
                currentPiece.x += dx;
                currentPiece.y += dy;
                return true;
            }
        }

        return false;
    }

    hardDrop(player) {
        const { currentPiece, board } = player;
        if (!currentPiece) return 0;

        let dropDistance = 0;
        while (this.isValidPosition(board, currentPiece.shape, currentPiece.x, currentPiece.y + 1)) {
            currentPiece.y++;
            dropDistance++;
        }

        player.score += dropDistance * 2;
        return dropDistance;
    }

    getGhostY(player) {
        const { currentPiece, board } = player;
        if (!currentPiece) return 0;

        let ghostY = currentPiece.y;
        while (this.isValidPosition(board, currentPiece.shape, currentPiece.x, ghostY + 1)) {
            ghostY++;
        }
        return ghostY;
    }

    startGame() {
        this.gameStatus = 'playing';
        this.winner = null;
        this.tickRate = 500;

        for (const player of this.players.values()) {
            player.board = this.createEmptyBoard();
            player.score = 0;
            player.level = 1;
            player.lines = 0;
            player.status = 'playing';
            player.bag = [];
            player.holdPiece = null;
            player.holdUsed = false;
            player.garbagePending = [];
            player.koCount = 0;
            player.lastDropAt = Date.now();
            player.lockStartedAt = null;
            this.spawnPiece(player);
        }

        this.broadcastState();

        this.gameLoop = setInterval(() => {
            this.gameTick();
        }, 50);
    }

    gameTick() {
        if (this.gameStatus !== 'playing') return;
        const now = Date.now();

        for (const player of this.players.values()) {
            if (player.status !== 'playing') continue;

            if (player.garbagePending.length > 0) {
                const pending = [...player.garbagePending];
                player.garbagePending = [];
                for (const packet of pending) {
                    this.addGarbage(player, packet.amount, packet.fromId);
                    if (player.status !== 'playing') break;
                }
            }

            const shouldDrop = now - (player.lastDropAt || 0) >= this.tickRate;
            const dropped = shouldDrop ? this.movePiece(player, 'down') : true;
            if (shouldDrop) player.lastDropAt = now;

            if (player.currentPiece && this.isPieceGrounded(player)) {
                if (!player.lockStartedAt) {
                    player.lockStartedAt = now;
                }
            } else {
                player.lockStartedAt = null;
            }

            if (!dropped && player.currentPiece && player.lockStartedAt && (now - player.lockStartedAt >= this.lockDelayMs)) {
                const result = this.lockPiece(player);
                player.lockStartedAt = null;

                if (result.garbage > 0) {
                    for (const p of this.players.values()) {
                        if (p.id !== player.id && p.status === 'playing') {
                            p.garbagePending.push({ amount: result.garbage, fromId: player.id });
                        }
                    }
                }

                if (player.status === 'playing') {
                    this.spawnPiece(player);
                }
            }
        }

        this.tickRate = 500;

        const playingPlayers = Array.from(this.players.values()).filter(p => p.status === 'playing');
        const finishedPlayers = Array.from(this.players.values()).filter(p => p.status === 'finished');

        // End match when only one player remains active.
        if (playingPlayers.length === 1 && finishedPlayers.length > 0) {
            this.winner = playingPlayers[0].id;
            this.endGame();
            return;
        }

        // If nobody is still playing, end the room's game.
        if (playingPlayers.length === 0 && finishedPlayers.length > 0) {
            this.endGame();
            return;
        }

        this.broadcastState();
    }

    endGame() {
        this.gameStatus = 'finished';
        if (this.gameLoop) {
            clearInterval(this.gameLoop);
            this.gameLoop = null;
        }
        this.broadcastState();
    }

    resetToLobby() {
        this.gameStatus = 'waiting';
        this.winner = null;
        for (const player of this.players.values()) {
            player.status = 'waiting';
            player.ready = false;
            player.board = this.createEmptyBoard();
            player.currentPiece = null;
            player.nextPiece = null;
            player.holdPiece = null;
            player.holdUsed = false;
            player.score = 0;
            player.level = 1;
            player.lines = 0;
            player.bag = [];
            player.garbagePending = [];
            player.koCount = 0;
            player.lockStartedAt = null;
        }
        this.broadcastState();
    }

    broadcastState() {
        const players = [];
        for (const [id, p] of this.players) {
            players.push({
                id: p.id,
                name: p.name,
                board: p.board,
                currentPiece: p.currentPiece ? {
                    type: p.currentPiece.type,
                    shape: p.currentPiece.shape,
                    x: p.currentPiece.x,
                    y: p.currentPiece.y,
                    ghostY: this.getGhostY(p)
                } : null,
                nextPiece: p.nextPiece,
                holdPiece: p.holdPiece,
                score: p.score,
                level: p.level,
                lines: p.lines,
                koCount: p.koCount,
                status: p.status,
                ready: p.ready,
                garbagePending: p.garbagePending.reduce((sum, packet) => sum + packet.amount, 0)
            });
        }

        io.to(this.code).emit('gameState', {
            players,
            gameStatus: this.gameStatus,
            winner: this.winner
        });
    }
}

const rooms = new Map();

function generateRoomCode() {
    return Math.random().toString(36).substring(2, 8).toUpperCase();
}

io.on('connection', (socket) => {
    console.log('Player connected:', socket.id);

    let currentRoom = null;
    let playerName = null;

    socket.on('createRoom', (name) => {
        const code = generateRoomCode();
        const room = new Room(code);
        rooms.set(code, room);
        const player = room.addPlayer(socket.id, name || `Player${Math.floor(Math.random() * 1000)}`);
        playerName = name;
        socket.join(code);
        currentRoom = room;
        socket.emit('roomCreated', { code, playerId: socket.id });
        room.broadcastState();
        console.log(`Room ${code} created by ${name}`);
    });

    socket.on('joinRoom', ({ code, name }) => {
        const room = rooms.get(code.toUpperCase());
        if (!room) {
            socket.emit('error', { message: 'Room not found' });
            return;
        }
        if (room.gameStatus === 'playing') {
            socket.emit('error', { message: 'Game already in progress' });
            return;
        }
        if (room.players.size >= 4) {
            socket.emit('error', { message: 'Room is full' });
            return;
        }

        const player = room.addPlayer(socket.id, name || `Player${Math.floor(Math.random() * 1000)}`);
        playerName = name;
        socket.join(code);
        currentRoom = room;
        socket.emit('roomJoined', { code, playerId: socket.id });
        room.broadcastState();
        console.log(`${name} joined room ${code}`);
    });

    socket.on('toggleReady', () => {
        if (!currentRoom) return;
        const player = currentRoom.players.get(socket.id);
        if (player) {
            player.ready = !player.ready;
            currentRoom.broadcastState();
        }
    });

    socket.on('startGame', () => {
        if (!currentRoom) return;
        const host = Array.from(currentRoom.players.values())[0];
        if (host.id !== socket.id) return;

        const readyPlayers = Array.from(currentRoom.players.values()).filter(p => p.ready);
        if (currentRoom.players.size >= 2 && readyPlayers.length >= 2) {
            currentRoom.startGame();
        } else if (currentRoom.players.size >= 2) {
            socket.emit('error', { message: 'Not all players are ready' });
        }
    });

    socket.on('playerInput', (input) => {
        if (!currentRoom || currentRoom.gameStatus !== 'playing') return;
        const player = currentRoom.players.get(socket.id);
        if (!player || player.status !== 'playing') return;

        let handled = false;

        switch (input.type) {
            case 'left':
                handled = currentRoom.movePiece(player, 'left');
                break;
            case 'right':
                handled = currentRoom.movePiece(player, 'right');
                break;
            case 'down':
                handled = currentRoom.movePiece(player, 'down');
                break;
            case 'rotateCW':
                handled = currentRoom.rotatePiece(player, 'CW');
                break;
            case 'rotateCCW':
                handled = currentRoom.rotatePiece(player, 'CCW');
                break;
            case 'hardDrop': {
                currentRoom.hardDrop(player);
                const result = currentRoom.lockPiece(player);
                if (result.garbage > 0) {
                    for (const p of currentRoom.players.values()) {
                        if (p.id !== player.id && p.status === 'playing') {
                            p.garbagePending.push({ amount: result.garbage, fromId: player.id });
                        }
                    }
                }
                if (player.status === 'playing') {
                    currentRoom.spawnPiece(player);
                }
                handled = true;
                break;
            }
            case 'hold':
                handled = currentRoom.holdCurrentPiece(player);
                break;
        }

        if (handled && player.currentPiece) {
            if (currentRoom.isPieceGrounded(player)) {
                player.lockStartedAt = Date.now();
            } else {
                player.lockStartedAt = null;
            }
        }

        currentRoom.broadcastState();
    });

    socket.on('resetGame', () => {
        if (!currentRoom) return;
        const host = Array.from(currentRoom.players.values())[0];
        if (host.id !== socket.id) return;
        currentRoom.resetToLobby();
    });

    socket.on('disconnect', () => {
        console.log('Player disconnected:', socket.id);
        if (currentRoom) {
            const roomCode = currentRoom.code;
            const isEmpty = currentRoom.removePlayer(socket.id);
            if (isEmpty) {
                rooms.delete(roomCode);
                console.log(`Room ${roomCode} deleted`);
            }
        }
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`Multiplayer Tetris server running on http://localhost:${PORT}`);
});
