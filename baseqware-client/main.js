"use strict";

const socket = new WebSocket("ws://localhost:8765");
const canvas = document.getElementById("canvas");
canvas.width = window.innerWidth;
canvas.height = window.innerHeight;
let windowPos = { x: 0, y: 0 };
let watermark = null;

// Retrieve team and enemy ESP settings from local storage
let teamEspEnabled = localStorage.getItem("teamEspEnabled") === "true";
let enemyEspEnabled = localStorage.getItem("enemyEspEnabled") === "true";

// Update the checkboxes in index.html based on the local storage values
document.getElementById("teamEsp").checked = teamEspEnabled;
document.getElementById("enemyEsp").checked = enemyEspEnabled;

// Add event listeners to the checkboxes to update the local storage values
document.getElementById("teamEsp").addEventListener("change", function () {
  teamEspEnabled = this.checked;
  localStorage.setItem("teamEspEnabled", teamEspEnabled);
});

document.getElementById("enemyEsp").addEventListener("change", function () {
  enemyEspEnabled = this.checked;
  localStorage.setItem("enemyEspEnabled", enemyEspEnabled);
});

window.addEventListener("resize", function () {
  canvas.width = window.innerWidth;
  canvas.height = window.innerHeight;
});

socket.addEventListener("open", function (event) {
  console.log("open");
  // Send screen size to the server
  const screenSize = {
    width: window.screen.width,
    height: window.screen.height,
  };
  socket.send(JSON.stringify(screenSize));
});

function clear_canvas() {
  const context = canvas.getContext("2d");
  context.clearRect(0, 0, canvas.width, canvas.height);
}

function draw_box(data) {
  if (data.is_enemy && !enemyEspEnabled) {
    return;
  }
  if (!data.is_enemy && !teamEspEnabled) {
    return;
  }
  const context = canvas.getContext("2d");
  context.strokeStyle = "white";
  context.lineWidth = 2;
  context.strokeRect(
    data.x - windowPos.x,
    data.y - windowPos.y,
    data.w,
    data.h
  );
}

document.getElementById("popup").addEventListener("click", function () {
  if (watermark) {
    watermark.close();
    watermark = null;
    return;
  }
  watermark = window.open(
    "popup.html",
    "popup",
    `popup=1,width=400,height=100,left=0,top=${screen.height - 100}`
  );
});

document.getElementById("pause").addEventListener("click", function () {
  socket.send(">pause");
});

socket.addEventListener("message", function (event) {
  console.log("message", event.data);

  windowPos.x = window.screenX;
  windowPos.y = window.screenY;
  // socket.send(JSON.stringify(windowPos));

  const data = JSON.parse(event.data);
  // console.log(data);
  if (Array.isArray(data)) {
    clear_canvas();
    for (let i = 0; i < data.length; i++) {
      draw_box(data[i]);
    }
  }
});

window.addEventListener("focus", function () {
  socket.send(">focus");
  let body = document.querySelector("body");
  body.classList.add("menu-toggled");
});

window.addEventListener("blur", function () {
  socket.send(">blur");
  let body = document.querySelector("body");
  body.classList.remove("menu-toggled");
});
