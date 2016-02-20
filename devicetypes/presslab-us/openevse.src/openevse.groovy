/**
 *  OpenEVSE
 *
 *  Copyright 2015 Ryan Press
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 *
 */
metadata {
	definition (name: "OpenEVSE", namespace: "presslab-us", author: "Ryan Press") {
		capability "Temperature Measurement"
		capability "Switch"
		capability "Configuration"
		capability "Energy Meter"
		capability "Power Meter"
		capability "Voltage Measurement"

		attribute "chargeLimit", "string"

		command "backlightOff"
		command "backlightOn"
		command "setChargeLimit"

		fingerprint endpointId: "08", profileId: "0104", deviceId: "0051", inClusters: "0000,0002,0006,0012,0702,0B04", outClusters: "0000"
	}

	simulator {
		// TODO: define status and reply messages here
	}

	tiles {
		standardTile("state", "device.state", width: 2, height: 2, canChangeIcon: true) {
			state "ready", label: '${name}', action: "switch.off", icon: "st.Transportation.transportation6", backgroundColor: "#44b621", nextState: "turningOff"
			state "connected", label: '${name}', action: "switch.off", icon: "st.Transportation.transportation6", backgroundColor: "#ffcc00", nextState: "turningOff"
			state "charging", label: '${name}', action: "switch.off", icon: "st.Transportation.transportation6", backgroundColor: "#0099ff", nextState: "turningOff"
			state "sleep", label: '${name}', action: "switch.on", icon: "st.Transportation.transportation6", backgroundColor: "#cc33ff", nextState: "turningOn"
			state "fault", label: '${name}', action: "switch.on", icon: "st.Transportation.transportation6", backgroundColor: "#ff0000", nextState: "turningOn"

			state "turningOn", label: '${name}', action: "switch.off", icon: "st.Transportation.transportation6", backgroundColor: "#44b621", nextState: "turningOff"
			state "turningOff", label: '${name}', action: "switch.on", icon: "st.Transportation.transportation6", backgroundColor: "#cc33ff", nextState: "turningOn"
		}

		valueTile("volts", "device.volts", width: 1, height: 1) {
			state("volts", label: '${currentValue}V', unit: "V")
		}

		valueTile("amps", "device.amps", width: 1, height: 1) {
			state("amps", label: '${currentValue}A', unit: "A")
		}

		standardTile("backlight", "device.backlight", width: 1, height: 1) {
			state "on", label: 'BACKLIGHT\r${name}', action: "backlightOff", icon: "st.switches.light.on", backgroundColor: "#ffffff", nextState: "off"
			state "off", label: 'BACKLIGHT\r${name}', action: "backlightOn", icon: "st.switches.light.off", backgroundColor: "#a6a6a6", nextState: "on"
		}

		valueTile("temperature", "device.temperature", width: 1, height: 1) {
			state("temperature", label:'${currentValue}°', unit:"F",
				backgroundColors:[
					[value: 120, color: "#44b621"],
					[value: 140, color: "#f1d801"],
					[value: 150, color: "#ff0000"]
				]
			)
		}

		valueTile("powerkw", "device.powerkw", width: 1, height: 1) {
			state("default", label: '${currentValue}kW')
		}

		valueTile("energy", "device.energy", width: 1, height: 1) {
			state("default", label: '${currentValue}kWh')
		}

		controlTile("limitSlider", "device.chargeLimit", "slider", height: 1, width: 2, range: "(5..21)") {
			state "setChargeLimit", action:"setChargeLimit", color:"#0099ff"
		}

		valueTile("chargeLimit", "device.chargeLimitText", height: 1, width: 1) {
			state "limit", label: '${currentValue}', backgroundColor:"#0099ff"
		}

		main(["state"])
		details(["state", "amps", "powerkw", "backlight", "temperature", "energy", "limitSlider", "chargeLimit"])
	}
}

def setChargeLimit(limit) {
	if (limit != null) {
		sendEvent(name: "chargeLimit", value: limit)
		if (limit == 21) {
        	log.debug "setChargeLimit(unlimited)"
			sendEvent(name: "chargeLimitText", value: "no limit")
            limit = 0xFFFFFF
        } else {
			log.debug "setChargeLimit({$limit})"
			sendEvent(name: "chargeLimitText", value: "${limit}kWh limit")
		}
		"st wattr 0x${device.deviceNetworkId} 8 ${CLUSTER_METERING} ${METERING_ATTR_LIMIT} ${TYPE_U24} {" + String.format('%06x', limit) + "}"
    }
}

def resetEnergy(limit) {
}

// Parse incoming device messages to generate events
def parse(String description) {
	log.debug "description is $description"

	// save heartbeat (i.e. last time we got a message from device)
	state.heartbeat = Calendar.getInstance().getTimeInMillis()

	def descMap = zigbee.parseDescriptionAsMap(description)
    
	if (descMap.cluster == "0002" && descMap.attrId == "0000") {
		sendEvent(name: "temperature", value: celsiusToFahrenheit(zigbee.convertHexToInt(descMap.value)).toInteger(), unit: "F")
	}

	if (descMap.cluster == "0012" && descMap.attrId == "0055") {
		switch (descMap.value) {
        case "0001":
			sendEvent(name: "switch", value: "on")
        	sendEvent(name: "state", value: "ready")
            break
        case "0002":
			sendEvent(name: "switch", value: "on")
        	sendEvent(name: "state", value: "connected")
            break
        case "0003":
			sendEvent(name: "switch", value: "on")
        	sendEvent(name: "state", value: "charging")
            break
        case "00fe":
        	sendEvent(name: "state", value: "sleep")
			sendEvent(name: "switch", value: "off")
            break
//        case "0000":
//        	sendEvent(name: "state", value: "fault")
//            break
		}
	} else if (descMap.cluster == "0702" && descMap.attrId == "0000") {
		sendEvent(name: "totalenergy", value: (zigbee.convertHexToInt(descMap.value) / (float)1000.0).toInteger(), unit: "kWh") // Convert from Wh to kWh
	} else if (descMap.cluster == "0702" && descMap.attrId == "0600") {
		sendEvent(name: "energy", value: (zigbee.convertHexToInt(descMap.value) / (float)1000.0).toInteger(), unit: "kWh") // Convert from Wh to kWh
	} else if (descMap.cluster == "0B04") {
		if (descMap.attrId == "0505") {
			if(descMap.value != "ffff")
				sendEvent(name: "volts", value: Math.round((zigbee.convertHexToInt(descMap.value) / (float)10.0)), unit: "V")
		} else if (descMap.attrId == "0508") {
			if(descMap.value != "ffff")
				sendEvent(name: "amps", value: zigbee.convertHexToInt(descMap.value) / (float)10.0, unit: "A")
		} else if (descMap.attrId == "050b") {
			if(descMap.value != "ffff")
				sendEvent(name: "powerkw", value: (zigbee.convertHexToInt(descMap.value) / (float)100.0).round(1), unit: "kW") // Convert from tens of W to kW
				sendEvent(name: "power", value: zigbee.convertHexToInt(descMap.value) * (float)10.0, unit: "W") // Convert from tens of W to W
		}
	}
}

def backlightOn() {
	log.debug "backlightOn()"
	sendEvent(name: "backlight", value: "on")

	"st cmd 0x${device.deviceNetworkId} 9 6 1 {}"
}

def backlightOff() {
	log.debug "backlightOff()"
	sendEvent(name: "backlight", value: "off")

	"st cmd 0x${device.deviceNetworkId} 9 6 0 {}"
}

def on() {
	log.debug "on()"
	"st cmd 0x${device.deviceNetworkId} 8 6 1 {}"
}

def off() {
	log.debug "off()"
	"st cmd 0x${device.deviceNetworkId} 8 6 0 {}"
}

private getCLUSTER_DEVTEMP() { 0x0002 }
private getCLUSTER_MULTISTATE() { 0x0012 }
private getCLUSTER_METERING() { 0x0702 }
private getCLUSTER_ELECMEAS() { 0x0B04 }

private getMULTISTATE_ATTR_VALUE() { 0x0055 }
private getDEVTEMP_ATTR_VALUE() { 0x0000 }
private getMETERING_ATTR_DEMAND() { 0x0600 }
private getMETERING_ATTR_LIMIT() { 0x0601 }
private getELECMEAS_ATTR_VOLTS() { 0x0505 }
private getELECMEAS_ATTR_AMPS() { 0x0508 }
private getELECMEAS_ATTR_WATTS() { 0x050B }

private getTYPE_U8() { 0x20 }
private getTYPE_U16() { 0x21 }
private getTYPE_U24() { 0x22 }
private getTYPE_U48() { 0x25 }
private getTYPE_S16() { 0x29 }


def configure() {
    def cmds =
        zigbee.configSetup("${CLUSTER_DEVTEMP}", "${DEVTEMP_ATTR_VALUE}",
                           "${TYPE_S16}", 120, 120, "{01}") +
        zigbee.configSetup("${CLUSTER_MULTISTATE}", "${MULTISTATE_ATTR_VALUE}",
                           "${TYPE_U16}", 1, 240, "{01}") +
        zigbee.configSetup("${CLUSTER_METERING}", "${METERING_ATTR_DEMAND}",
                           "${TYPE_U24}", 180, 180, "{01}") +
        zigbee.configSetup("${CLUSTER_ELECMEAS}", "${ELECMEAS_ATTR_VOLTS}",
                           "${TYPE_U16}", 2, 60, "{01}") +
        zigbee.configSetup("${CLUSTER_ELECMEAS}", "${ELECMEAS_ATTR_AMPS}",
                           "${TYPE_U16}", 2, 60, "{01}") +
        zigbee.configSetup("${CLUSTER_ELECMEAS}", "${ELECMEAS_ATTR_WATTS}",
                           "${TYPE_S16}", 2, 60, "{01}")
    log.info "configure() --- cmds: $cmds"
    return cmds
}

private getEndpointId() {
	new BigInteger(device.endpointId, 16).toString()
}
