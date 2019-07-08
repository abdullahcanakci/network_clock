
var screenSettings = document.getElementById("settings_div");
var screenInfo = document.getElementById("info_div");
var screenHome = document.getElementById("home_div");

var screens = [screenHome, screenSettings, screenInfo];

var deviceState;

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
    fetch("http://192.168.1.103/api").then(function(response) {
        response.text().then(function(text) {
          fillpage(text);
        });
    });
}

function fillpage(jsonResponse){
    console.log(jsonResponse);
    deviceState = JSON.parse(jsonResponse);
    var tz = deviceState.timezone / 60.0;

    parseTime(deviceState.time);

    document.getElementById("timezone").innerHTML = "+" + tz;
    document.getElementById("connection").innerHTML = deviceState.ssid;
    document.getElementById("ssid").value = deviceState.ssid;
    document.getElementById("ssid_password").value = deviceState.psk;
    document.getElementById("timezone").value = tz;
    document.getElementById("brightness").value = deviceState.bright;
    document.getElementById("stationname").value = deviceState.dname;
    document.getElementById("stationpassword").value = deviceState.dpass;
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