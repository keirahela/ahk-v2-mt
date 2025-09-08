const { spawn } = require("child_process");
const fs = require("fs");

function captureOutput(command, args) {
  return new Promise((resolve, reject) => {
    const cmd = spawn(command, args);

    let output = ""; // Variable to store the output

    // Capture stdout in real-time
    cmd.stdout.on("data", (data) => {
      output += data.toString();
      // Check if the desired URL is present in the output
      if (output.includes("https://") && output.includes("trycloudflare.com")) {
        const urlMatch = output.match(
          /https:\/\/[a-zA-Z0-9.-]+\.trycloudflare\.com/
        );
        if (urlMatch) {
          console.log("Found URL:", urlMatch[0]);
          fs.writeFileSync("output.txt", urlMatch[0]);
        }
      }
    });

    // Capture stderr (if any error occurs)
    cmd.stderr.on("data", (data) => {
      output += data.toString(); // Append stderr data as it comes
      // Check if the desired URL is present in the output
      if (output.includes("https://") && output.includes("trycloudflare.com")) {
        const urlMatch = output.match(
          /https:\/\/[a-zAZ0-9.-]+\.trycloudflare\.com/
        );
        if (urlMatch) {
          console.log("Found URL:", urlMatch[0]);
          fs.writeFileSync("output.txt", urlMatch[0]);
        }
      }
    });

    // Handle the command exit
    cmd.on("exit", (code) => {
      console.log(`Child process exited with code ${code}`);
      if (code !== 0) {
        reject(`Command failed with exit code ${code}`);
      }
    });

    // Keep the process running
    // (You can also add a manual mechanism to stop it, such as a keyboard input or specific timeout)
    setInterval(() => {
      console.log("Tunnel is still running...");
    }, 5000); // Just logging periodically to keep it active
  });
}

// Run your command (example)
captureOutput("cloudflared", ["tunnel", "--url", "http://localhost:8080"])
  .then(() => {
    console.log("Tunnel is running indefinitely...");
  })
  .catch((err) => {
    console.error("Error:", err);
  });
