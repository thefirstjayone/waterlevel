const channelID = 3026172;
const apiKey = "AHHPO1T2TZDGZ2G8"; // ThingSpeak Read Key
const refreshInterval = 10000; // 10 seconds

let lastLevel = null;
let lastUpdateTime = Date.now();

function fetchWaterLevel() {
    fetch(`https://api.thingspeak.com/channels/${channelID}/fields/1/last.json?api_key=${apiKey}`)
        .then(response => response.json())
        .then(data => {
            const level = parseInt(data.field1);
            updateTank(level);
            lastUpdateTime = Date.now();
        })
        .catch(err => console.error("Error fetching data:", err));
}

function updateTank(level) {
    const tankHeight = 400;
    const tankWidth = 200;

    // Handle special case for sensor not detected
    if (level === 2) {
        document.getElementById("statusMessage").textContent = "Colour Sensor Not Detected";
    } else {
        document.getElementById("statusMessage").textContent = "";
    }

    // Set water color
    let color;
    if (level >= 100) color = "rgba(0,255,0,0.5)";
    else if (level >= 75) color = "rgba(0,0,255,0.5)";
    else if (level >= 50) color = "rgba(255,255,0,0.5)";
    else if (level >= 25) color = "rgba(255,0,0,0.5)";
    else color = "rgba(255,0,0,0.5)";

    const wavePath = generateWave(level, tankWidth, tankHeight);

    const waveElement = document.getElementById("waterWave");
    waveElement.setAttribute("d", wavePath);
    waveElement.setAttribute("fill", color);

    // Flash effect below 25%
    if (level < 25) {
        waveElement.classList.add("flash");
    } else {
        waveElement.classList.remove("flash");
    }

    // Trigger slosh animation only if level changes
    if (lastLevel !== null && level !== lastLevel) {
        triggerSlosh();
    }

    lastLevel = level;
    document.getElementById("waterLevelText").textContent = level + "%";
}

function generateWave(level, width, height) {
    const waterHeight = height * (1 - level / 100);
    const waveHeight = 10;
    const waveLength = 50;
    const points = [];
    const now = Date.now() / 500;

    for (let x = 0; x <= width; x += 1) {
        const y = waterHeight + Math.sin((x / waveLength) * 2 * Math.PI + now) * waveHeight;
        points.push(`${x},${y}`);
    }
    points.push(`${width},${height}`, `0,${height}`);
    return "M" + points.join(" ") + " Z";
}

function triggerSlosh() {
    let startTime = Date.now();
    let duration = 3000; // 3 seconds
    function animate() {
        const elapsed = Date.now() - startTime;
        if (elapsed > duration) return;
        requestAnimationFrame(animate);
        const sloshHeight = 15;
        const sloshLength = 40;
        const waveElement = document.getElementById("waterWave");
        const level = lastLevel;
        const height = 400;
        const width = 200;
        const waterHeight = height * (1 - level / 100);
        const now = Date.now() / 200;
        const points = [];

        for (let x = 0; x <= width; x += 1) {
            const y = waterHeight + Math.sin((x / sloshLength) * 2 * Math.PI + now) * sloshHeight;
            points.push(`${x},${y}`);
        }
        points.push(`${width},${height}`, `0,${height}`);
        waveElement.setAttribute("d", "M" + points.join(" ") + " Z");
    }
    animate();
}

function updateTimer() {
    const seconds = Math.floor((Date.now() - lastUpdateTime) / 1000);
    document.getElementById("lastUpdated").textContent = `Last updated: ${seconds} seconds ago`;
}

setInterval(fetchWaterLevel, refreshInterval);
setInterval(updateTimer, 1000);

fetchWaterLevel();
