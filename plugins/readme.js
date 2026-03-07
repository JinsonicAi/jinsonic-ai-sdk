/**
	buttonType:"rtspBtn" | "faceBtn"
	formList:{
		type:"input" | "select" | "checkbox" | "radio" | "switch" ,
	}
**/
const aa = {
	"component": {
		"parentType": "custom-input",  // component type （for example input components）
		"label": "rtsp-input", // the name of the node（for example rtsp input）
		"type": "RtspNode", // node type（sole）
		"formList": [ //node parameters
			//input box
			{
				"type": "input", //input box
				"name": "rtsp-url", // enter a name for the field
				"key": "rtsp_url", // enter the key corresponding to the field
				"default": "", // input box defaults
				"buttonType": "rtspBtn", // button type
				"unit": "s" // the input box is followed by a description
			},
			//drop down box
			{
				"type": "select", //drop down box
				"name": "decode-location", // the name of the drop down box
				"key": "decode_location", // the key in the drop down list
				"default": "local", // default value of the drop down box corresponds to the value in the preceding list
				"unit": "s", // the drop down box is followed by a description
				"list": [ // drop down box for a list of options
					{
						"label": "local", // the name of the drop down option
						"value": "local" // the value of the drop down box option
					}
				]
			},
			//checkbox
			{
				"type": "checkbox", //checkbox
				"name": "decode-location", // the name of the check box
				"key": "decode_location", //check box
				"default": [], // default value corresponds to the value in the previous list
				"list": [ // checkbox option list
					{
						"label": "local", // the name of the check box
						"value": "local" // checkbox corresponding value
					}
				]
			},
			//radio box
			{
				"type": "radio", //radio box
				"name": "decode-location", //the name of the radio box
				"key": "decode_location", //the key corresponding to the radio box
				"default": "local", // default value corresponds to the value in the previous list
				"list": [ // a list of radio box options
					{
						"label": "local", //the name of the radio box
						"value": "local" // the value of the radio box
					}
				]
			},
			//switch
			{
				"type": "switch", //switch
				"name": "decode-location", // switch name
				"key": "decode_location", // the key corresponding to the switch
				"default": true, //drop down box default value default value true or false
				"checkboxList": [ // checkbox option list
					{
						"label": "local", // the name of the check box
						"value": "local" // checkbox corresponding value
					}
				]
			},
		]
	}
}