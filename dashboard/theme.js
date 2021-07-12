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
	switch (themeSelection) {
		case 'light':
		case 'dark':
		case 'oled':
			localStorage.setItem('theme', themeSelection);
			setClass(themeSelection);
			alertify.success(`Theme changed to ${themeSelection}!`);
			break;
		default:
			console.error('Invalid theme given');
			alertify.error(`Something went wrong when saving your preference: '${themeSelection}' is not a valid theme argument.`);
			break;
	}
}
