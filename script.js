let lastUpdateTime = Date.now();

function updateTank(level) {
    const water = document.getElementById("water");
    const status = document.getElementById("status");

    // Set water height
    water.style.height = level + "%";

    // Color based on level
    if (level >= 100) water.style.background = "rgba(0,255,0,0.4)";
    else if (level >= 75) water.style.background = "rgba(0,128,255,0.4)";
    else if (level >= 50) water.style.background = "rgba(255,255,0,0.4)";
    else if (level >= 25) water.style.background = "rgba(255,0,0,0.4)";
    else water.style.background = "rgba(255,0,0,0.4)";

    // Flash below 25%
    if (level < 25) {
        water.style.animation = "slosh 4s ease-in-out infinite, flash 1s infinite";
    } else {
        water.style.animation = "slosh 4s ease-in-out infinite";
    }

    // Status message
    if (level === 2) {
        status.innerHTML = "Colour Sensor Not Detected";
    } else {
        status.innerHTML = `Water Level: ${level}%`;
    }

    lastUpdateTime = Date.now();
}

function fetchData() {
    // Replace this with your ThingSpeak or MQTT fetch
    // Simulated level for demo
    const simulatedLevel = Math.floor(Math.random() * 101);
    updateTank(simulatedLevel);
}

// Update seconds since last update
setInterval(() => {
    const seconds = Math.floor((Date.now() - lastUpdateTime) / 1000);
    document.getElementById("update-seconds").innerText = seconds;
}, 1000);

// Refresh data every 10 seconds
setInterval(fetchData, 10000);

// Initial fetch
fetchData();
