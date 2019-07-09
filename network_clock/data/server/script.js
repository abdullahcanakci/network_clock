
var screenSettings = document.getElementById("settings_div");
var screenInfo = document.getElementById("info_div");
var screenHome = document.getElementById("home_div");

var screens = [screenHome, screenSettings, screenInfo];

var brightnessSlider = document.getElementById("brightness");

var networkName = document.getElementById("ssid");
var networkPass = document.getElementById("ssid_password");
var deviceName = document.getElementById("station_name");
var loginName = document.getElementById("login_name");
var devicePassword = document.getElementById("station_password");
var timezone = document.getElementById("set_timezone");

brightnessSlider.addEventListener("change", updateBrightness);

var deviceState;
var curTime;
window.setInterval(function(){
    if(curTime == null){
        return;
    }
    curTime = curTime + 1;
    parseTime(curTime);
}, 1000);

function go(id){
    screens.forEach(element => {
        element.classList.remove("invisible");
        element.classList.remove("visible");
    });

    if(id == "home_div"){
        screenSettings.classList.add("invisible");
        screenInfo.classList.add("invisible");
    }
    else if(id == "settings_div"){
        screenInfo.classList.add("invisible");
        screenHome.classList.add("invisible");
    }
    else if(id == "info_div"){
        screenSettings.classList.add("invisible");
        screenHome.classList.add("invisible");
    }
    document.getElementById(id).classList.add("visible");
}

function onPageLoad(){
    loadPage();
}

function loadPage(){
    console.log("Load page");
    fetch("/api").then(function(response) {
        response.text().then(function(text) {
          fillpage(text);
        });
    });
}

function fillpage(jsonResponse){
    console.log(jsonResponse);
    deviceState = JSON.parse(jsonResponse);
    var tz = deviceState.timezone / 60.0;

    curTime = deviceState.time;

    document.getElementById("timezone").innerHTML = "+" + tz;
    document.getElementById("connection").innerHTML = deviceState.ssid;
    networkName.value = deviceState.ssid;
    networkPass.value = deviceState.psk;
    timezone.value = deviceState.timezone;
    brightnessSlider.value = deviceState.bright;
    deviceName.value = deviceState.dname;
    loginName.value = deviceState.lname;
    devicePassword.value = deviceState.dpass;
}

function parseTime(seconds){
    var hour, minute, second;
    second = seconds % 60;
    minute = Math.floor(seconds / 60) % 60;
    hour = Math.floor(seconds / (3600)) % 24;

    second = padZero(second);
    minute = padZero(minute);
    hour = padZero(hour);
    
    document.getElementById("clock").innerHTML = hour + ":" + minute + ":" + second;
}

// Pads clock outputs to have 09:05:07 instead of 9:5:7
function padZero(num){
    var s = num  + "";
    if(s.length != 2) s = "0" + s;
    return s;
}

function updateBrightness(){
    sendJSON(
        JSON.stringify({
            type : 0,
            bright: brightnessSlider.value
        }),
        true
    );
}

function saveNetworkInfo(){
    sendJSON(
        JSON.stringify({
            type : 1,
            ssid : networkName.value,
            psk : networkPass.value
        }),
        true
    );
}

function saveDeviceInfo(){
    sendJSON(JSON.stringify({
        type : 2,
        dname : deviceName.value,
        lname : loginName.value,
        dpass : devicePassword.value,
        bright : brightnessSlider.value,
        timezone : timezone.value
        }),
        true
    );
    
}

function saveServerInfo(){
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/api", true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify({
        type : 3
    }));
    sendJSON(
        JSON.stringify({
            type : 3
        }),
        true
    );
}

function sendJSON(payload, reload) {
    fetch('/api', {
        method: 'post',
        headers: {
            'Accept': 'application/json, text/plain, */*',
            'Content-Type': 'application/json'
        },
        body: payload
    }).then(reload ? loadPage(): nul);
}