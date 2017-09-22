function get(url, success) {
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.onload = () => {
        if (xhr.status === 200) {
            success(JSON.parse(xhr.responseText));
        }
        else {
            alert('Request failed.  Returned status of ' + xhr.status);
        }
    };
    xhr.send();
}

function post(url, data, success) {
    const xhr = new XMLHttpRequest();

    xhr.open('POST', url);
    xhr.onload = () => {
        if (xhr.status === 200) {
            success(JSON.parse(xhr.responseText));
        }
        else if (xhr.status !== 200) {
            alert('Request failed.  Returned status of ' + xhr.status);
        }
    };
    xhr.send(JSON.stringify(data));
}

const baseUrl = "http://192.168.0.169";
let currentStatus: WifiStatus;

enum WifiStatus {
    Idle,
    NoSsidAvailable,
    ScanComplete,
    Connected,
    ConnectFailed,
    ConnectionLost,
    Disconnected
}

function el(id): any {
    return document.getElementById(id);
}

function updateStatus() {
    setLoading(true);
    get(baseUrl + "/wifi/status", s => {
        el("ip").innerText = s.ip;
        el("status").innerText = WifiStatus[s.status];

        [].forEach.call(document.getElementsByClassName("network"), e => {
            const data = JSON.parse(e.dataset['j']);

            if (s.ssid == data.ssid) {
                if (s.status == WifiStatus.Connected) {
                    e.children[0].innerHTML = "<div class='connected'></div>";
                } else {
                    e.children[0].innerHTML = "<div class='spinner-donut'></div>";
                }
            }
        });
        if(s.status != WifiStatus.Connected) { setTimeout(() => updateStatus(), 1000);}
        setLoading(false);
    });
}

function setLoading(loading) {
    if (loading) {
        el("networksLoading").classList.remove("hidden");
        el("buttons").classList.add("hidden");
    } else {
        el("networksLoading").classList.add("hidden");
        el("buttons").classList.remove("hidden");
    }
}

function refreshNetworks() {
    setLoading(true);
    get(baseUrl + "/wifi", nwks => {
        el('networksBody').innerHTML = "";
        let html = "";
        nwks.forEach(i => {
            html += `<tr class='network' data-j=${JSON.stringify(i)}>
                      <td></td>
                      <td>${i.ssid} (${i.secure ? "S" : ""})</td>
                      <td>${i.rssi}</td>
                    </tr>`;
        });
        el('networksBody').innerHTML = html;
        [].forEach.call(document.getElementsByClassName("network"), e => {
            e.addEventListener("click", e => {
                const data = JSON.parse(e.currentTarget.dataset['j']);
                el('ssid').value = data.ssid;
                if (data.secure) {
                    el('password').setAttribute('required', null);
                } else {
                    el('password').removeAttribute('required');
                }
            });
        });
        setLoading(false);
        updateStatus();
    });
}

el("rescan").addEventListener('click', refreshNetworks);
el("updateStatus").addEventListener('click', updateStatus);
el("connect").onclick = () => {
    if (!el('ssid').checkValidity() || !el('password').checkValidity()) {
        return true;
    }
    console.log(el('ssid').value, el('password').value);
    post(baseUrl + "/wifi", {ssid: el('ssid').value, password: el('password').value}, _ => {
        updateStatus();
    });
    return false;
};
refreshNetworks();