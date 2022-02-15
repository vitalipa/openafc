import * as React from "react";
import { Alert, FormGroup, InputGroup, TextInput, InputGroupText, AlertActionCloseButton} from "@patternfly/react-core";
import {Table, TableHeader, TableBody, TableVariant } from "@patternfly/react-table";
import { FreqRange } from "../Lib/RatApiTypes";

/**
 * AllowedRangesForm.tsx: Displays admin defined allowed frequency ranges as readonly values
 */


export const defaultRanges =   [           
    {       
        name : "UNII-5",
        startFreqMHz : 5925,
        stopFreqMHz : 6425
    },      
    {       
        name : "UNII-7",
        startFreqMHz : 6525,
        stopFreqMHz : 6875
    }               
]

const cols = ["Name", "Low Frequency", "High Frequency"]


const freqBandToRow = (f: FreqRange, index) => ({
    id: index,
    cells: [ f.name, f.startFreqMHz, f.stopFreqMHz]
})

/**
 * Sub form component for allowed freq ranges
 */
export class AllowedRangesDisplay extends React.PureComponent<{data: FreqRange[]}, {showWarn: boolean}> {

    constructor(props) {
        super(props)
        this.state = {
            showWarn:  !this.props.data || this.props.data.length === 0
        }
    }


    private renderTable = (datasource : FreqRange[]) => {
        return (
        <Table aria-label="freq-table" variant={TableVariant.compact}  cells={cols as any} rows={datasource.map(freqBandToRow)} >
                <TableHeader/>
                <TableBody/>
                
        </Table>)
    }

    render() {
        let dataSource = !this.props.data || this.props.data.length === 0 ? defaultRanges : this.props.data
        return (<>
            {this.state.showWarn ? <Alert title={'Error Fetching Allowed Frequency Ranges'} variant="warning" action={<AlertActionCloseButton onClose={() => this.setState({ showWarn: false, })}/> }>
                <pre>Falling back to default UNII-5 and UNII-7 ranges</pre>
            </Alert> : false }
            <FormGroup
                label="Allowed Frequency Ranges"
                fieldId="allowedFreqGroup">
                {this.renderTable(dataSource)}
            
        </FormGroup> 
        </>
        )
    }
}