// ThingSpeak config
const channelID = 3026172; // your channel ID
const readAPIKey = "AHHPO1T2TZDGZ2G8"; // your read API key
const fieldNumber = 1; // assuming field1 holds the water level

function fetchWaterLevel() {
  fetch(`https://api.thingspeak.com/channels/${channelID}/fields/${fieldNumber}/last.json?api_key=${readAPIKey}`)
    .then(response => response.json())
    .then(data => {
      const level = parseFloat(data.field1);
      updateTank(level);
    })
    .catch(err => console.error("Error fetching data:", err));
}

function updateTank(level) {
  const water = document.querySelector('.water');
  const levelText = document.querySelector('.level-text');

  // Update height
  water.style.height = Math.min(level, 100) + '%';

  // Update text
  levelText.textContent = `Water Level: ${level}%`;

  // Remove all color/flash classes
  water.classList.remove('green', 'blue', 'yellow', 'red', 'flash');

  // Assign color based on level
  if (level >= 100) {
    water.classList.add('green');
  } else if (level >= 75) {
    water.classList.add('blue');
  } else if (level >= 50) {
    water.classList.add('yellow');
  } else if (level >= 25) {
    water.classList.add('red');
  } else {
    water.classList.add('red', 'flash'); // below 25% = flashing red
  }
}

// Refresh every 15 seconds
setInterval(fetchWaterLevel, 15000);
fetchWaterLevel();
