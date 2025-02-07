# VideoScript
This project contains various scripts mainly for 
- video processing
  - videoScripts
     - TS video processing

## video Scripts
Common build command: `g++ <cppfile> -o <executable> -std=c++1y`

### File Naming
(Not standardized yet)

### Files description
- anc2038Parser
    * Ready to use
    * Parse TS video with st-2038 data pid to extract types of data and metadata of video
- getPcr
    * Not yet confirmed
    * Get all packets PCR in TS video if found any
- update_ts_pid
    * Not yet confirmed
    * Modify target TS PID of the TS video
