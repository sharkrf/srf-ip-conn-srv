const htmlElement = document.getElementsByTagName('html')[0];

let theme = localStorage.getItem('theme');
if (!theme) localStorage.setItem('theme', 'light'); // Light is the default mode
else setClass(theme); // Set the theme to the one fetched in localStorage


function setClass(selection) {
	switch (selection) {
		case 'light':
			htmlElement.removeAttribute('class');
			break;
		case 'dark':
		case 'oled':
			htmlElement.setAttribute('class', selection);
			break;
		default:
			console.error('Invalid theme argument');
			alertify.error('Invalid theme argument');
			break;
	}
}

function switchTheme(themeSelection) {
	themeSelection = themeSelection.toLowerCase()
	switch (themeSelection) {
		case 'light':
		case 'dark':
		case 'oled':
			localStorage.setItem('theme', themeSelection);
			setClass(themeSelection);
			alertify.success(`Theme changed to ${themeSelection}!`);
			break;
		default:
			console.error('Invalid argument, check the following table for the correct arguments');
			console.table({
				"1": {
					"Color mode": "Light mode",
					"argument": "light"
				},
				"2": {
					"Color mode": "Dark mode",
					"argument": "dark"
				},
				"3": {
					"Color mode": "Super dark/OLED optimized",
					"argument": "oled"
				}
			});
			break;
	}
}
