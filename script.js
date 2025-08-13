const tank = document.getElementById("tank");
const water = document.getElementById("water");
const percentageText = document.getElementById("percentage");
const sensorStatus = document.getElementById("sensor-status");
const lastUpdateEl = document.getElementById("last-update");

let lastUpdateTime = null;

function updateTank(level) {
    // Set water color
    if (level >= 100) {
        water.style.backgroundColor = "#00ff00"; // green
    } else if (level >= 75) {
        water.style.backgroundColor = "#0000ff"; // blue
    } else if (level >= 50) {
        water.style.backgroundColor = "#ffff00"; // yellow
    } else if (level >= 25) {
        water.style.backgroundColor = "#ff0000"; // red
    } else {
        water.style.backgroundColor = "#ff0000"; // red (low)
        water.classList.add("flash");
    }

    if (level >= 25) {
        water.classList.remove("flash");
    }

    // Adjust water height
    water.style.height = level + "%";

    // Update percentage text
    percentageText.textContent = level + "%";

    // Show sensor status if exactly 2%
    if (level === 2) {
        sensorStatus.textContent = "Colour Sensor Not Detected";
    } else {
        sensorStatus.textContent = "";
    }
}

function fetchData() {
    fetch("https://api.thingspeak.com/channels/3026172/fields/1/last.json?api_key=AHHPO1T2TZDGZ2G8")
        .then(response => response.json())
        .then(data => {
            const level = parseInt(data.field1);
            updateTank(level);

            lastUpdateTime = new Date();
            updateLastUpdateDisplay();
        })
        .catch(error => {
            console.error("Error fetching data:", error);
        });
}

function updateLastUpdateDisplay() {
    if (!lastUpdateTime) return;

    const secondsAgo = Math.floor((new Date() - lastUpdateTime) / 1000);
    lastUpdateEl.textContent = `Last update: ${secondsAgo} seconds ago`;
}

setInterval(fetchData, 10000); // refresh every 10 seconds
setInterval(updateLastUpdateDisplay, 1000); // update counter every second

// Initial load
fetchData();
