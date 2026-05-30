# 🔋 MoVE - Electric Motorcycle Telemetry System

![Dashboard Preview](/src/img/dashboard.png)

> **Real-time API and Dashboard for Voltz Electric Motorcycle Monitoring**
> A complete solution for reading, processing, and visualizing telemetry data through the CAN network.

---

# 🚀 Overview

**MoVE** is a full-stack system that combines a **RESTful API** with an **interactive web dashboard** to monitor critical parameters of a **Voltz electric motorcycle** in real time.

The platform was developed to support performance analysis, battery monitoring, vehicle tracking, and CAN network diagnostics, making it suitable for field testing, firmware development, maintenance, and data analysis.

---

# 📦 Features

## ✅ Interactive Web Dashboard

* Current speed (km/h)
* Battery level (%) and SoC (State of Charge)
* Estimated range (km)
* Battery temperature (°C)
* Riding mode (Eco, Normal, Sport)
* Instant power consumption (kW)
* Real-time location tracking using OpenStreetMap and Leaflet
* CAN message log (ID, raw data, timestamp)

## ✅ RESTful API

* Structured endpoints for vehicle telemetry data
* Receives and processes CAN frames via WebSocket or HTTP POST
* Standardized JSON responses
* Simulation mode with mock data support
* Optional API Key authentication

## ✅ CAN Network Integration

* CAN 2.0B message acquisition and decoding
* Mapping of CAN IDs to vehicle signals (speed, voltage, current, etc.)
* Conversion of raw CAN data into physical values

---

# 🛠️ Technologies Used

| Layer         | Technology                |
| ------------- | ------------------------- |
| Frontend      | HTML, CSS, JavaScript     |
| Backend       | Node.js, Express, MongoDB |
| Communication | REST API, WebSocket       |
| Maps          | OpenStreetMap, Leaflet    |
| Deployment    | Kubernetes (K3s)          |

---

# 📁 Project Structure

> 🚧 Under Development

---

# 🔧 Installation and Setup

## 1. Install Git

### Windows

Visit the official Git website:

https://git-scm.com

Download the Windows installer and follow the recommended installation settings.

### macOS

Using Homebrew:

```bash
brew install git
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install git
```

---

## 2. Install Node.js and npm

Node.js is a JavaScript runtime environment, while npm (Node Package Manager) is used to manage project dependencies.

### Windows and macOS

Visit the official Node.js website:

https://nodejs.org

Download and install the **LTS (Long-Term Support)** version.

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y nodejs
sudo apt-get install -y npm
```

### Linux (Fedora)

```bash
sudo dnf install nodejs
sudo dnf install npm
```

---

## 3. Clone the Repository

```bash
git clone https://github.com/AlexsandroJ/apiVoltz.git
```

---

## 4. Navigate to the Project Directory

```bash
cd apiVoltz
```

---

## 5. Install Project Dependencies

```bash
npm install
```

---

## 6. Run the Application

Start the development server:

```bash
npm run dev
```

---

# 📖 API Documentation

The API is documented using **Swagger (OpenAPI)**.

After starting the server, access:

```text
http://localhost:3000/api-docs
```

This endpoint is available in the development environment and provides interactive API documentation.

---

# 🎯 Use Cases

* Electric vehicle telemetry monitoring
* CAN bus diagnostics and reverse engineering
* Battery performance analysis
* Fleet monitoring and management
* Embedded systems testing
* Research and academic projects involving electric mobility

---

# 📄 License

This project is currently under development. License information will be added in future releases.
