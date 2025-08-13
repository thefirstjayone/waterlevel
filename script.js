const water = document.getElementById("water");
const percentageText = document.getElementById("percentage");
const sensorStatus = document.getElementById("sensor-status");
const lastUpdateEl = document.getElementById("last-update");

let lastUpdateTime = null;

function updateTank(level) {
    // Water colour (with wave pattern)
    let color;
    if (level >= 100) color = "rgba(0, 180, 0, 0.5)";
    else if (level >= 75) color = "rgba(0, 100, 200, 0.5)";
    else if (level >= 50) color = "rgba(200, 200, 0, 0.5)";
    else if (level >= 25) color = "rgba(200, 50, 50, 0.5)";
    else color = "rgba(200, 50, 50, 0.5)";

    water.style.backgroundImage = `
        radial-gradient(circle at 25% 25%, rgba(255,255,255,0.2), transparent 40%),
        radial-gradient(circle at 75% 75%, rgba(255,255,255,0.15), transparent 40%),
        linear-gradient(to top, ${color} 0%, ${color} 100%)
    `;

    // Flash effect below 25%
    if (level < 25) water.classList.add("flash");
    else water.classList.remove("flash");

    // Adjust height
    water.style.height = level + "%";

    // Text percentage
    percentageText.textContent = level + "%";

    // Sensor message
    sensorStatus.textContent = (level === 2) ? "Colour Sensor Not Detected" : "";
}

function fetchData() {
    fetch("https://api.thingspeak.com/channels/3026172/fields/1/last.json?api_key=AHHPO1T2TZDGZ2G8")
        .then(res => res.json())
        .then(data => {
            const level = parseInt(data.field1);
            updateTank(level);
            lastUpdateTime = new Date();
            updateLastUpdateDisplay();
        })
        .catch(err => console.error("Fetch error:", err));
}

function updateLastUpdateDisplay() {
    if (!lastUpdateTime) return;
    const secondsAgo = Math.floor((new Date() - lastUpdateTime) / 1000);
    lastUpdateEl.textContent = `Last update: ${secondsAgo} seconds ago`;
}

setInterval(fetchData, 10000); // update every 10s
setInterval(updateLastUpdateDisplay, 1000); // update timer

fetchData();
