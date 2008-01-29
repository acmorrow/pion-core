dojo.require("dijit.layout.StackContainer");
dojo.require("dojo.parser");	// scan page for widgets and instantiate them

var user_config_page_initialized = false;

var init = function() {
	initReactorConfigPage();
}

dojo.addOnLoad(init);

dojo.subscribe("main_stack_container-selectChild", configPageSelected);

function configPageSelected(page) {
	console.debug('Selected ' + page.title + ' configuration page');
	if (page.title == "Users" && !user_config_page_initialized) {
		initUserConfigPage();
		user_config_page_initialized = true;
	}
}
