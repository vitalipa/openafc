import { AvailableSpectrumInquiryRequest, AvailableSpectrumInquiryResponse, DeploymentEnum } from "./RatAfcTypes";
import { RatResponse, success, error } from "./RatApiTypes";
import { guiConfig, getDefaultAfcConf } from "./RatApi";

/**
 * RatAfcApi.ts
 * API functions for RAT AFC
 */

/**
 * Call RAT AFC resource
 */
export const spectrumInquiryRequest = (request: AvailableSpectrumInquiryRequest): Promise<RatResponse<AvailableSpectrumInquiryResponse>> =>
    fetch(guiConfig.rat_afc, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ version: "1.1", availableSpectrumInquiryRequests: [request]})
    })
    .then(async resp => {
        if (resp.status == 200) {
            const data = (await resp.json()) as { version: string, availableSpectrumInquiryResponses: AvailableSpectrumInquiryResponse[] };
            const response = data.availableSpectrumInquiryResponses[0];
            if (response.response.responseCode == 0) {
                return success(data.availableSpectrumInquiryResponses[0]);
            } else {
                return error(response.response.shortDescription, response.response.responseCode, response);
            }
        } else {
            return error(resp.statusText, resp.status, resp);
        }
    })
    .catch(e => {
        return error("encountered an error when running request", undefined, e);
    })

export const sampleRequestObject: AvailableSpectrumInquiryRequest = {
    requestId: "0",
    deviceDescriptor: {
        serialNumber: "sample-ap",
        certificationId: [
            {
                nra: "FCC",
                id: "1234567890"
            }
        ],
        rulesetIds: ["47_CFR_PART_15_SUBPART_E"]
    },
    location: {
        ellipse: {
            center: {
                latitude: 41,
                longitude: -74
            },
            majorAxis: 200,
            minorAxis: 100,
            orientation: 90
        },
        elevation:{
            height: 15,
            verticalUncertainty: 5,
            heightType: "AGL"
        },
        
        indoorDeployment: DeploymentEnum.indoorDeployment
    },
    minDesiredPower: 15,
    vendorExtensions: [{
        extensionID: "RAT v1.1AFC Config",
        parameters: getDefaultAfcConf()
    }],
    inquiredChannels: [
        {
            globalOperatingClass: 133,
        },
        {
            globalOperatingClass: 134,
            channelCfi: [15, 47, 79]
        }
    ],
    inquiredFrequencyRange: [
        { lowFrequency: 5925000000, highFrequency: 6425000000 }
    ]
}