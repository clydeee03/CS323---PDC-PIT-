# Multiplayer Tetris - Specification

## 1. Project Overview

**Project Name:** MultiTetris
**Type:** Real-time multiplayer web game
**Core Functionality:** Classic Tetris gameplay with real-time synchronization where multiple players compete simultaneously, clearing lines to send garbage to opponents.
**Target Users:** Casual gamers looking for competitive local/multiplayer Tetris experience

## 2. Technology Stack

- **Backend:** Node.js with Express.js
- **Real-time Communication:** Socket.io
- **Frontend:** Vanilla JavaScript with HTML5 Canvas
- **Architecture:** Client-server model with authoritative server

## 3. Visual Specification

### Color Palette
- **Background:** `#0a0a0f` (deep black)
- **Grid Background:** `#1a1a24` (dark slate)
- **Grid Lines:** `#2a2a3a` (subtle grid)
- **Text Primary:** `#e0e0e0` (light gray)
- **Text Accent:** `#00ff88` (neon green)
- **Accent Secondary:** `#ff6b6b` (coral red)

### Tetromino Colors
- **I-piece:** `#00f5ff` (cyan)
- **O-piece:** `#ffdd00` (yellow)
- **T-piece:** `#b44fff` (purple)
- **S-piece:** `#44ff44` (green)
- **Z-piece:** `#ff4444` (red)
- **J-piece:** `#4477ff` (blue)
- **L-piece:** `#ff8844` (orange)

### UI Components
- Game board: 10 columns × 20 rows
- Each cell: 28px × 28px with 2px gap
- Ghost piece: 30% opacity preview
- Next piece preview: 4×4 grid on the side
- Player list panel: right sidebar
- Room code display: top center

### Typography
- **Font Family:** "JetBrains Mono", monospace
- **Headings:** 24px bold
- **Body:** 14px regular
- **Score display:** 18px bold

## 4. Game Mechanics

### Tetromino System
- Standard 7 tetrominoes (I, O, T, S, Z, J, L)
- Random bag system (each piece appears once per bag)
- Spawn position: top-center of board

### Movement & Rotation
- Left/Right arrow: Move horizontally
- Down arrow: Soft drop (accelerated fall)
- Up arrow: Rotate clockwise
- Space: Hard drop (instant drop)
- Z key: Rotate counter-clockwise

### Line Clearing
- Single: 100 points, sends 0 garbage
- Double: 300 points, sends 2 garbage
- Triple: 500 points, sends 4 garbage
- Tetris (4 lines): 800 points, sends 6 garbage

### Garbage System
- Incoming garbage adds incomplete lines at bottom
- Garbage lines have random gaps for clearing
- Visual indicator when garbage is incoming

### Scoring
- Points per piece placed: 10
- Line clear bonus multiplied by level
- Level increases every 10 lines cleared

### Speed/Level Progression
- Level 1: 1000ms per drop
- Each level: drop speed decreases by 10%
- Max speed cap at level 15

## 5. Multiplayer Features

### Room System
- Create room with unique 6-character code
- Join existing room by code
- 2-4 players per room
- Host can start game when 2+ players ready

### Synchronization
- Server-authoritative game state
- Client sends inputs only
- Server broadcasts game state at 30fps
- Inputs processed server-side for fairness

### Player States
- Waiting: In lobby, ready indicator
- Playing: Active game
- Finished: Game over (board full)
- Winner: Last player standing

### Game End Conditions
- Any player reaches 1000 points
- All boards filled (last standing wins)

## 6. Network Protocol

### Events (Client → Server)
- `createRoom`: Create new game room
- `joinRoom`: Join existing room by code
- `playerInput`: Send input (keypress)
- `toggleReady`: Toggle ready state
- `startGame`: Host starts the game

### Events (Server → Client)
- `roomCreated`: Room code and player info
- `playerJoined`: New player notification
- `playerLeft`: Player disconnected
- `gameState`: Full board state (30fps)
- `gameOver`: Game ended, final scores
- `error`: Connection/game errors

### Data Structures

```javascript
// Game State (broadcast every tick)
{
  players: [
    {
      id: string,
      name: string,
      board: number[][], // 20×10 grid
      currentPiece: { type, rotation, x, y },
      nextPiece: { type },
      score: number,
      level: number,
      lines: number,
      status: 'playing' | 'finished'
    }
  ],
  gameStatus: 'waiting' | 'playing' | 'finished',
  winner: string | null
}

// Player Input
{
  type: 'left' | 'right' | 'down' | 'rotateCW' | 'rotateCCW' | 'drop' | 'hardDrop'
}
```

## 7. Frontend UI Layout

```
┌──────────────────────────────────────────────────────┐
│                    MULTITETRIS                       │
│                   Room: ABC123                       │
├────────────────────────────┬─────────────────────────┤
│                            │  Players                │
│                            │  ─────────────         │
│      [GAME BOARD]          │  ● Player1  ✓ Ready    │
│       10 × 20              │  ○ Player2              │
│                            │                         │
│                            │  Next:                  │
│                            │  ┌─────────┐           │
│                            │  │ [piece] │           │
│                            │  └─────────┘           │
│                            │                         │
│                            │  Score: 1250           │
│                            │  Level: 3               │
│                            │  Lines: 15              │
├────────────────────────────┴─────────────────────────┤
│  Controls: ←→ Move | ↑ Rotate | ↓ Soft | SPACE Hard  │
└──────────────────────────────────────────────────────┘
```

## 8. Acceptance Criteria

1. ✅ Multiple players can create and join rooms
2. ✅ All players see synchronized game state
3. ✅ Tetrominoes fall and can be controlled
4. ✅ Line clearing works and awards points
5. ✅ Garbage lines sent to opponents on clears
6. ✅ Game ends when conditions met
7. ✅ Winner declared and displayed
8. ✅ Reconnection handled gracefully
9. ✅ Responsive controls with no noticeable lag
10. ✅ Visual feedback for all actions
