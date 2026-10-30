// Task 5 Dashboard: CSV Export & Chart Analytics Module
import { getDatabase, ref, get, onValue } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-database.js";

// One-Click CSV Export Function
window.exportHistoricDataToCSV = async () => {
  const db = getDatabase();
  const btn = document.getElementById("csv-download-btn");
  btn.innerText = "Querying Logs...";
  btn.disabled = true;

  try {
    const snapshot = await get(ref(db, "logs"));
    if (!snapshot.exists()) {
      alert("No historical records found in Firebase RTDB!");
      btn.innerText = "Download CSV Logs";
      btn.disabled = false;
      return;
    }

    const data = snapshot.val();
    let csvContent = "Timestamp (Epoch),Date (UTC),Temperature (C),Humidity (%),LDR (Raw),Bulb State,Operating Mode\r\n";

    // Iterate through all push ID keys
    for (let key in data) {
      const row = data[key];
      const isoDate = new Date(row.timestamp * 1000).toISOString();
      const line = [
        row.timestamp,
        `"${isoDate}"`,
        row.temperature,
        row.humidity,
        row.ldr,
        row.bulb ? "ON" : "OFF",
        row.mode
      ].join(",");
      csvContent += line + "\r\n";
    }

    // Create browser Blob and trigger download
    const blob = new Blob([csvContent], { type: "text/csv;charset=utf-8;" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `iot_telemetry_export_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);

    btn.innerText = "CSV Download Complete!";
    setTimeout(() => {
      btn.innerText = "Download CSV Logs";
      btn.disabled = false;
    }, 2500);

  } catch (err) {
    console.error("Export Error:", err);
    alert("Export failed: " + err.message);
    btn.innerText = "Download CSV Logs";
    btn.disabled = false;
  }
};