const { invoke } = window.__TAURI__.core;

let scanButton;
let tagIdEl;
let tagKindEl;
let resultArea;
let errorMsgEl;

async function scan() {
  errorMsgEl.textContent = "";
  resultArea.classList.add("result-hidden");
  scanButton.disabled = true;
  scanButton.textContent = "Scanning...";

  try {
    const tag = await invoke("scan_rfid");
    console.log("Tag received from Rust:", tag);
    tagIdEl.textContent = tag.id;
    tagKindEl.textContent = tag.kind;
    resultArea.classList.remove("result-hidden");
  } catch (e) {
    console.error(e);
    errorMsgEl.textContent = String(e);
  } finally {
    scanButton.disabled = false;
    scanButton.textContent = "Scan RFID Tag";
  }
}

window.addEventListener("DOMContentLoaded", () => {
  scanButton = document.querySelector("#scan-button");
  tagIdEl = document.querySelector("#tag-id");
  tagKindEl = document.querySelector("#tag-kind");
  resultArea = document.querySelector("#result-area");
  errorMsgEl = document.querySelector("#error-msg");

  scanButton.addEventListener("click", () => {
    scan();
  });
});
