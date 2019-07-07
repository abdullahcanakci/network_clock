
var screenSettings = document.getElementById("settings_div");
var screenInfo = document.getElementById("info_div");
var screenHome = document.getElementById("home_div");

var screens = [screenHome, screenSettings, screenInfo];

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

}