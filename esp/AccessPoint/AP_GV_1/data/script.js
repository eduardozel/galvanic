// script.js - Логика для взаимодействия с ESP32 WebSocket

//        showStatusMessage('Only .cfg or .txt files are allowed', 3000);

        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;


        function showStatusMessage(text, duration = 5000) {
          const statusDiv = document.getElementById('statusMessage');
          if (!statusDiv) return;
          statusDiv.innerText = text;
          statusDiv.style.display = 'block';
          setTimeout(() => {statusDiv.style.display = 'none';}, duration);
        } // showStatusMessage


        window.addEventListener('load', onLoad);
        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage; // <-- add this line
        }
        function onOpen(event) {
            console.log('Connection opened');
        }
        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            console.log(event.data);
            var data = JSON.parse(event.data);
            document.getElementById('v1').innerText = data.v1;
            document.getElementById('v2').innerText = data.v2;

            document.getElementById('c1').innerText = data.c1;
            document.getElementById('c2').innerText = data.c2;

            const cnt1 = data.timer1;
            if ( cnt1 > 0 ) {
              const min1 = Math.floor( cnt1 / 60);
              const sec1 = cnt1 - min1 * 60;
              document.getElementById('counter1').innerText = min1 + ':' + String(sec1).padStart(2, '0');
            } else {
              document.getElementById('counter1').innerText = '- - -';
            }
            const cnt2 = data.timer2;
            if ( cnt2 > 0 ) {
              const cnt2 = data.timer2;
              const min2 = Math.floor( cnt2 / 60);
              const sec2 = cnt2 - min2 * 60;
              document.getElementById('counter2').innerText = min2 + ':' + String(sec2).padStart(2, '0');;
            } else {
              document.getElementById('counter2').innerText = '- - -';
            }
        } // onMessage

        function loadCOMBOValues(
        ) {
          fetch('/config_values')
            .then(response => {
              if (!response.ok) throw new Error('HTTP ' + response.status);
              return response.json();
            })
            .then(data => {
              const val1 = document.getElementById('val1');
              const val2 = document.getElementById('val2');
            
              for (let i = 0; i < data.values.length; i++) {
                  const opt1 = document.createElement('option');
                  opt1.value = data.values[i];
                  opt1.text  = data.display[i];
                  val1.add(opt1);
                  
                  const opt2 = document.createElement('option');
                  opt2.value = data.values[i];
                  opt2.text  = data.display[i];
                  val2.add(opt2);
              }; // for

            })  // .then(data
          ; // fetch
        } // loadCOMBOValues

        function onLoad(event) {
            initWebSocket();
            initButton();
            loadCOMBOValues();
            setInterval(getState, 10000);
        } // onLoad

        function getState() {
            websocket.send(JSON.stringify({ act: 'getState'}));
        }

        function initButton() {
            document.getElementById('start1').addEventListener('click', btn_start1);
            document.getElementById('start2').addEventListener('click', btn_start2);
            document.getElementById('stop1').addEventListener('click',  btn_stop1);
            document.getElementById('stop2').addEventListener('click',  btn_stop2);
        }

        function btn_start1() {
            const val = document.getElementById('val1').value;
            const duration = ''+(Number(document.getElementById('hr1').value)+Number(document.getElementById('mn1').value));
            websocket.send(JSON.stringify({ act: 'start1', val, duration }));
            showStatusMessage('btn_start #1', 3000);
        } // btn_start1

        function btn_start2() {
            const val = document.getElementById('val2').value;
            const duration = ''+(Number(document.getElementById('hr2').value)+Number(document.getElementById('mn2').value));
            websocket.send(JSON.stringify({ act: 'start2', val, duration}));
            showStatusMessage('btn_start #2', 3000);
        } // btn_start2

        function btn_stop1() {
            websocket.send(JSON.stringify({ act: 'stop1' }));
        }

        function btn_stop2() {
            websocket.send(JSON.stringify({ act: 'stop2' }));
        }

