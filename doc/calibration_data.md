## Calibration Data

### API
 
- Factory Provision Prepare
- Factory Provision Append Data
- Factory Provision Verify
- Factory Read Item

### Factory TLV's

`[tag][length][data][crc32]`

`tag`: 4 bytes (e.g. 'HWID' or 'FCAL')  
`length`: uint32 length of data section, network order  
`data`: byte array  
`crc32`: uint32, network order  

#### HWID

- Tag: `HWID`
- Data format: JSON (hwid.schema.json)

#### FCAL

- Tag: `FCAL`
- Data format: JSON (fcal.schema.json)