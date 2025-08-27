// script.js - Логика для взаимодействия с ESP32 WebSocket

        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
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

			const cnt = data.timer;
			if ( cnt > 0 ) {
				const min = Math.floor( cnt / 60);
				const sec = cnt - min * 60;
				document.getElementById('counter').innerText = min + ':' + String(sec).padStart(2, '0');
			} else {
				document.getElementById('counter').innerText = '- - -';
			}

			var redInput   = document.getElementById('red');
			var greenInput = document.getElementById('green');
			var blueInput  = document.getElementById('blue');

			var red   = parseInt(data.red,   10) || 0;
			var green = parseInt(data.green, 10) || 0;
			var blue  = parseInt(data.blueInput,  10) || 0;

        // range 0-255
            red   = Math.max(0, Math.min(255, red));
			green = Math.max(0, Math.min(255, green));
			blue  = Math.max(0, Math.min(255, blue));

			redInput.value   = red;
			greenInput.value = green;
			blueInput.value  = blue;
        } // onMessage(

        function onLoad(event) {
            initWebSocket();
            initButton();
            setInterval(getState, 10000);
		}

        function getState() {
            websocket.send(JSON.stringify({ act: 'getState'}));
        }

        function initButton() {
            document.getElementById('start').addEventListener('click', btn_start);
            document.getElementById('stop').addEventListener('click',  btn_stop);
            document.getElementById('mode').addEventListener('change', toggleColors);
		} // initButton()

        function btn_start() {
		
			var redInput   = document.getElementById('red');
			var greenInput = document.getElementById('green');
			var blueInput = document.getElementById('blue');

			var red   = parseInt(redInput.value,   10) || 0;
			var green = parseInt(greenInput.value, 10) || 0;
			var blue  = parseInt(blueInput.value,  10) || 0;


        // range 0-255
            red   = Math.max(0, Math.min(255, red));
			green = Math.max(0, Math.min(255, green));
			blue  = Math.max(0, Math.min(255, blue));

			redInput.value   = red;
			greenInput.value = green;
			blueInput.value  = blue;
			
            const brightness = document.getElementById('brightness').value;
			const duration = ( parseInt(document.getElementById('hr').value,   10) || 0 ) + Number(document.getElementById('mn').value);
            const mode = document.getElementById('mode').value;
            websocket.send(JSON.stringify({ act: 'start', brightness, duration, mode, red, green, blue }));
        }

        function btn_stop() {
            websocket.send(JSON.stringify({ act: 'stop' }));
        } // btn_stop
		
		function toggleColors() {
            var modeSelect = document.getElementById('mode');
            var customRow = document.getElementById('colors');

			if (modeSelect.value === 'custom') {
				colors.style.display = '';  // Показать (сброс display к дефолтному)
			} else {
				colors.style.display = 'none';
			}
    } // toggleColors

