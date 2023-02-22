// definování proměnných
var canvas = document.getElementById("canvas");
var ctx = canvas.getContext("2d");
var width = canvas.width;
var height = canvas.height;
var x = width / 2;
var y = height / 2;
var dx = 0;
var dy = 0;
var speed = 2;
var friction = 0.95;

// definování funkce pro kreslení hráče
function drawPlayer() {
  ctx.beginPath();
  ctx.arc(x, y, 10, 0, Math.PI * 2);
  ctx.fillStyle = "red";
  ctx.fill();
  ctx.closePath();
}

// definování funkce pro pohyb hráče
function movePlayer() {
  if (keys[38]) {
    // šipka nahoru
    dy -= speed;
  }
  if (keys[40]) {
    // šipka dolů
    dy += speed;
  }
  if (keys[37]) {
    // šipka vlevo
    dx -= speed;
  }
  if (keys[39]) {
    // šipka vpravo
    dx += speed;
  }

  // přidání tření
  dx *= friction;
  dy *= friction;

  // aktualizace pozice
  x += dx;
  y += dy;

  // kontrola hraničních podmínek
  if (x > width) {
    x = 0;
  }
  if (x < 0) {
    x = width;
  }
  if (y > height) {
    y = 0;
  }
  if (y < 0) {
    y = height;
  }
}

// definování funkce pro vykreslení pozadí
function drawBackground() {
  ctx.fillStyle = "lightblue";
  ctx.fillRect(0, 0, width, height);
}

// definování funkce pro hlavní herní smyčku
function gameLoop() {
  drawBackground();
  drawPlayer();
  movePlayer();
}

// přiřazení funkce pro zpracování klávesových stisků
var keys = [];
window.addEventListener("keydown", function (event) {
  keys[event.keyCode] = true;
});
window.addEventListener("keyup", function (event) {
  keys[event.keyCode] = false;
});

// spuštění herní smyčky každých 10 milisekund
setInterval(gameLoop, 10);
