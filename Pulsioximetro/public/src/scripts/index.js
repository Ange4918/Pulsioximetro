const loginElement = document.querySelector('#login-form');
const contentElement = document.querySelector("#content-sign-in");
const userDetailsElement = document.querySelector('#user-details');
const authBarElement = document.querySelector("#authentication-bar");
const cardElement = document.querySelector(".card");
const containertElement = document.querySelector(".container");

// Elements for sensor readings
const heartRateElement = document.getElementById("heart-rate");
const spO2Element = document.getElementById("spo2");


// MANAGE LOGIN/LOGOUT UI
const setupUI = (user) => {
  if (user) {
    //toggle UI elements
    containertElement.style.display = "none";
    cardElement.style.display = 'none';
    loginElement.style.display = 'none';
    contentElement.style.display = 'block';
    authBarElement.style.display ='block';
    userDetailsElement.style.display ='block';
   /*  userDetailsElement.innerHTML = user.email; */

    // get user UID to get data from database
    var uid = user.uid;
    console.log(uid);

    // Database paths (with user UID)
    var dbPathHeartRate = 'UsersData/' + uid.toString() + '/heartRate';
    var dbPathSpO2 = 'UsersData/' + uid.toString() + '/spo2';
   

    // Database references
    var dbRefHeartRate = firebase.database().ref().child(dbPathHeartRate);
    var dbRefSpO2 = firebase.database().ref().child(dbPathSpO2);
   

    // Update page with new readings
    dbRefHeartRate.on('value', snap => {
      heartRateElement.innerText = snap.val().toFixed(2);
    });

    dbRefSpO2.on('value', snap => {
      spO2Element.innerText = snap.val().toFixed(2);
    });


  // if user is logged out
  } else{
    // toggle UI elements
    containertElement.style.display = 'flex';
    cardElement.style.display = 'block';
    loginElement.style.display = 'block';
    authBarElement.style.display ='none';
    userDetailsElement.style.display ='none';
    contentElement.style.display = 'none';
  }
}