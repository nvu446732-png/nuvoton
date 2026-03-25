const express = require('express');
const fs = require('fs');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

const DATA_FILE = 'rfid_data.json';

// Đọc dữ liệu từ file JSON
function getData() {
    if (!fs.existsSync(DATA_FILE)) {
        return { users: {}, scans: [] };
    }
    try {
        const content = fs.readFileSync(DATA_FILE, 'utf-8');
        return JSON.parse(content);
    } catch (err) {
        console.error("Lỗi đọc file JSON:", err);
        return { users: {}, scans: [] };
    }
}

// Ghi dữ liệu vào file JSON
function saveData(data) {
    try {
        fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
    } catch (err) {
        console.error("Lỗi ghi file JSON:", err);
    }
}

// API nhận UID từ ESP32/Nuvoton
app.post('/api/rfid', (req, res) => {
    const { uid } = req.body;
    if (!uid) return res.status(400).json({ error: "Missing UID" });

    const data = getData();
    const name = data.users[uid] || null;

    const record = {
        uid,
        name: name,
        timestamp: new Date().toISOString()
    };

    data.scans.push(record);
    saveData(data);

    console.log(`Đã nhận thẻ: ${uid} - ${name || 'Khách lạ'}`);
    res.status(200).json({ message: "Saved", name });
});

// API lấy lịch sử quẹt thẻ
app.get('/api/history', (req, res) => {
    const data = getData();
    // Trả về danh sách scan, đảo ngược để cái mới nhất lên đầu
    res.json([...data.scans].reverse());
});

// API gán người dùng mới
app.post('/api/assign-user', (req, res) => {
    const { uid, name } = req.body;
    if (!uid || !name) return res.status(400).json({ error: "Missing fields" });

    const data = getData();
    data.users[uid] = name;
    saveData(data);

    res.json({ message: "User assigned successfully" });
});

const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Server đang chạy tại http://localhost:${PORT}`);
});