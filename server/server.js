const localtunnel = require("localtunnel");
const WebSocket = require("ws");

const port = 8080;
const wss = new WebSocket.Server({ port });

console.log(`WebSocket server is running at ws://localhost:${port}`);

const players = new Map();

wss.on("connection", (ws) => {
  console.log("New client connected");

  const playerId = generateUniqueId();
  players.set(playerId, { status: "connected", ws });

  ws.send(JSON.stringify({ action: "set_unique_id", id: playerId }));

  broadcast({
    sender: ws,
    action: "player_connected",
    playerId,
    status: "connected",
  });

  ws.on("message", (message) => {
    message = message.toString();
    let parsedMessage;
    try {
      parsedMessage = JSON.parse(message);
      console.log(parsedMessage);
    } catch (e) {
      console.error("Invalid JSON message received:", message);
      return;
    }

    const { action, playerId, status } = parsedMessage;

    console.log(playerId);

    switch (action) {
      case "update_status":
        if (players.has(playerId) && players.get(playerId).status != status) {
          players.get(playerId).status = status;

          broadcast({
            sender: ws,
            action: "status_updated",
            playerId,
            status,
          });

          console.log(`${playerId} status updated to ${status}`);

          if (status === "left") {
            broadcast({ sender: ws, action: "player_left", playerId });
          }

          allReadyCheck();
        }
        break;

      case "all_ready_check":
        const allReady = [...players.values()].every(
          (player) => player.status === "ready"
        );
        if (allReady) {
          broadcast({ sender: ws, action: "all_ready" });
        }
        break;

      default:
        console.log("Unknown action:", action);
    }
  });

  ws.on("close", () => {
    console.log(`Client disconnected: ${playerId}`);
    if (players.has(playerId)) {
      players.delete(playerId);

      broadcast({ sender: ws, action: "player_disconnected", playerId });
    }
  });
});

function allReadyCheck() {
  const allReady = [...players.values()].every(
    (player) => player.status === "ready"
  );
  if (allReady) {
    broadcast({ sender: ws, action: "all_ready" });
  }
}

function generateUniqueId() {
  return "player-" + Math.random().toString(36).substring(2, 9);
}

function broadcast(message) {
  const messageString = JSON.stringify(message);
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN && message.sender != client) {
      client.send(messageString);
    }
  });
}
