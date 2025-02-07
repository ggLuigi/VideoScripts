/**
 * @file update_ts_pid.cpp
 * @author your name (you@domain.com)
 * @brief
 * ./program changePid inputfilename --out <> --oldPid oldPid --newPid newPid
 * (optional) --pmtPid pmtPid
 * (optional) --from packetno --to packetno
 * ./program removePid inputfilename --out <> pid
 * (optional) --from packetno --to packetno
 * ./program changePmtPid inputfilename --out <> --oldPid oldPid --newPid newPid
 * (optional) --from packetno --to packetno
 * @version 0.1
 * @date 2022-02-18
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int get_input_output_file(char *argv[], int argc, std::string &input_file, std::string &output_file);
int convert_to_binary(unsigned int num);
unsigned int get_packet_pid(unsigned char ts_packet[]);
void write_pid(unsigned char ts_packet[], int newPid, int offset);
void modify_pat_pmtpid(int packet_offset, unsigned char ts_packet[], int oldPid, int newPid);
void changePid(char *argv[], int argc, int commandIdx, std::string const &input_file, std::string const &output_file);
void get_pmt_pid_list(int payload_offset, unsigned const char ts_packet[], std::vector<int> &pmt_pid_list);
bool modify_pmt_if_contain_oldpid(int payload_offset, unsigned const char ts_packet[], int oldPid);
void changePmtPid(char *argv[], int argc, int commandIdx, std::string const &input_file, std::string const &output_file);
void update_arguments_to_lower(int argc, char *argv[], int commandIdx);
void get_arguments(int argc, char *argv[], int commandIdx, int *const oldPid, int *const newPid, int *const pmtPid, int *const fromPacket, int *const toPacket);
void print_packet(unsigned const char packet[], int packet_size);
std::string get_filename();
void print_usage();

// return commandIdx to indicate whether it has specified output filename
int get_input_output_file(char *argv[], int argc, std::string &input_file, std::string &output_file)
{
    if (argc < 3)
    {
        printf("Please specify input file name\n");
        print_usage();
        throw std::exception();
    }
    int commandIndex = 2; // 0-based index
    input_file = argv[commandIndex];
    // ifstream is used for reading files
    std::ifstream inf{input_file};
    if (!inf)
    {
        std::cerr << "Fail to open the input file: " << input_file << '\n';
        print_usage();
        throw std::exception();
    }

    // find output file
    commandIndex++;             // check if there is '--out'
    if (argc == commandIndex || // no more arguments or no specified '--out' flag
        (argc >= commandIndex + 2 &&
         strcmp(argv[commandIndex], "--out") != 0))
    {
        size_t filename_index = input_file.find_last_of("/\\");
        if (filename_index == std::string::npos)
        {
            filename_index = 0;
        }
        size_t file_ext = input_file.find_last_of(".");
        output_file = input_file.substr(0, filename_index) + input_file.substr(filename_index, file_ext - filename_index) + "_output" + input_file.substr(file_ext);
        // printf("out filename_index: %d, ext: %d\n", filename_index, file_ext);
    }
    else if (argc >= commandIndex + 2 &&
             strcmp(argv[commandIndex], "--out") == 0)
    {
        output_file = argv[commandIndex + 1];
        commandIndex += 2;
    }
    else
    {
        printf("Cannot get the output filename\n");
        // print_usage();
        throw std::exception();
    }
    printf("The input file is %s, the output file is %s\n", input_file.c_str(), output_file.c_str());

    // ofstream is used for writing files
    std::ofstream outf{output_file};

    // if we couldn't open the file
    if (!outf)
    {
        std::cerr << "Fail to open output file: " << output_file << '\n';
        throw std::exception();
    }
    return commandIndex;
}

int convert_to_binary(unsigned int num)
{
    if (num <= 1)
        return num;
    int binary = 0;
    // printf("received num %d\n", num);
    int bits = 0;
    while (num >= pow(2, bits + 1)) // if num is >= next bit, then need to increment the bit to have that flag
    {
        bits++;
    }
    // printf("the bits is %d\n", bits);
    for (int i = bits; i >= 0; i--)
    {
        int mask = pow(2, i);
        // printf("the mask is %d\n", mask);
        if (num & mask)
            binary += 1 * pow(10, i);
    }
    return binary;
}

unsigned int get_packet_pid(unsigned char ts_packet[])
{
    // get the 13-bit pid located in (0-based) [11-19bit] [1-2byte]
    // printf("the first MSB: %d\n", convert_to_binary(ts_packet[1] & 0x1f));
    // printf("the second byte: %d\n", convert_to_binary(ts_packet[2]));
    unsigned int pid = (ts_packet[1] & 0x1f) << 8 | ts_packet[2];
    // printf("the pid in binary: %d (%d)\n", convert_to_binary(pid), pid);
    return pid;
}

void write_pid(unsigned char ts_packet[], int newPid, int offset)
{
    // printf("before writing the PID ->%X %X\n", ts_packet[offset], ts_packet[offset + 1]);
    // set the first byte
    int leftMost = (newPid & (0x1F << 8)) >> 8; // get new pid left most 5 bits of 13 bits
    leftMost |= ((0xE0) & ts_packet[offset]);   // combine with first 3-bits
    ts_packet[offset] = leftMost;
    // set the second byte
    int rightMost = newPid & 0xFF;
    ts_packet[offset + 1] = rightMost;
    // printf("wrote the PID ->%X %X\n", ts_packet[offset], ts_packet[offset + 1]);
}

int skip_adaptation_packet(unsigned char ts_packet[])
{
    int byte_index = 4;
    int adaptation_field_control = ts_packet[3] & 0x30;
    if (adaptation_field_control == 2)
    {
        // no payload
        return -1;
    }
    else if (adaptation_field_control == 3) // adaptation field present if 10 or 11
    {
        // skip the adaptation field
        // get the adaptation_length
        int adaptation_field_length = ts_packet[byte_index];
        byte_index += adaptation_field_length + 1;
        printf("skip adaptation field length - %d\n", byte_index);
    }
    return byte_index;
}

void get_pmt_pid_list(int payload_offset, unsigned const char ts_packet[], std::vector<int> &pmt_pid_list)
{
    // find all the program pid
    int program_num_byte_start = payload_offset + 9;
    int psi_section_length = (ts_packet[payload_offset + 2] & 0x0F) << 8 | ts_packet[payload_offset + 3]; // 12bits across 3rd - 4th byte
    for (int i = 0; i < ((psi_section_length - 9) / 4); i++)                                              // loop every program
    {
        program_num_byte_start += 2;
        int pid = (ts_packet[program_num_byte_start] & 0x1f) << 8 | ts_packet[program_num_byte_start + 1];
        pmt_pid_list.push_back(pid);
    }
}

void modify_pat_pmtpid(int payload_offset, unsigned char ts_packet[], int oldPid, int newPid)
{
    // payload_offset should be now at pointer_field
    int program_num_byte_start = payload_offset + 9;
    int psi_section_length = (ts_packet[payload_offset + 2] & 0x0F) << 8 | ts_packet[payload_offset + 3]; // 12bits across 3rd - 4th byte
    for (int i = 0; i < ((psi_section_length - 9) / 4); i++)                                              // loop every program
    {
        program_num_byte_start += 2;
        int pid = (ts_packet[program_num_byte_start] & 0x1f) << 8 | ts_packet[program_num_byte_start + 1];
        if (pid == oldPid)
        {
            write_pid(ts_packet, newPid, program_num_byte_start);
            break;
        }
    }
}

bool modify_pmt_if_contain_oldpid(int payload_offset, unsigned char ts_packet[], int oldPid, int newPid)
{
    // payload_offset should be now at pointer_field
    int section_length_position_end = payload_offset + 3;              // at 3rd-4th byte
    int program_number = ts_packet[section_length_position_end + 1] << 8 | ts_packet[section_length_position_end + 2];
    int pcrPid = (ts_packet[section_length_position_end + 6] & 0x1F) << 8 | ts_packet[section_length_position_end + 7];
    if (pcrPid == oldPid) {
        // printf("Change also pcr pid as it is same as oldPid\n");
        write_pid(ts_packet, newPid, section_length_position_end + 6);
        int newPcrPid = (ts_packet[section_length_position_end + 6] & 0x1F) << 8 | ts_packet[section_length_position_end + 7];
        // printf("Validate new pcr pid: [%d]\n", newPcrPid);
    }
    int program_info_length_pos_end = section_length_position_end + 9; // at 15th-16th byte
    int psi_section_length = (ts_packet[section_length_position_end - 1] & 0x0F) << 8 | ts_packet[section_length_position_end];
    int program_info_length = (ts_packet[program_info_length_pos_end - 1] & 0x0F) << 8 | ts_packet[program_info_length_pos_end];
    int stream_elements_byte_start = program_info_length_pos_end + program_info_length + 1;
    int stream_elements_byte_index = 0; // goes through the bytes inside the loop from start
    // loop every elementary stream
    int stream_elements_length = psi_section_length - 4 - (stream_elements_byte_start - section_length_position_end - 1);
    // printf("section_leng_pos_end: %d; prog_info_leng_pos: %d; psi_section_leng: %d; prog_info_leng: %d; stream_elements_byte_start: %d\n", section_length_position_end, program_info_length_pos_end, psi_section_length, program_info_length, stream_elements_byte_start);
    // printf("stream_elements_length: %d\n", stream_elements_length);

    while (stream_elements_length - stream_elements_byte_index > 0)
    {
        int pid_packet_offset = stream_elements_byte_start + stream_elements_byte_index + 1;
        int pid = (ts_packet[pid_packet_offset] & 0x1f) << 8 | ts_packet[pid_packet_offset + 1];
        int es_info_length_pos_end = stream_elements_byte_index + stream_elements_byte_start + 4;
        int es_info_length = (ts_packet[es_info_length_pos_end - 1] & 0x0F) << 8 | ts_packet[es_info_length_pos_end];
        // printf("es_info_length pos: %d, length: %d\n", es_info_length_pos_end, es_info_length);
        if (pid == oldPid)
        {
            write_pid(ts_packet, newPid, pid_packet_offset);
            return true;
        }
        stream_elements_byte_index += 5;
        // skip ES_info_length
        stream_elements_byte_index += es_info_length;
        // printf("The remaining length: %d\n", stream_elements_length - stream_elements_byte_index);
    }
    return false;
}

void changePid(char *argv[], int argc, int commandIdx, std::string const &input_file, std::string const &output_file)
{
    printf("ChangePid\n");
    if (argc <= commandIdx + 3)
    {
        printf("Please input required arguments for 'changePid'\n");
        print_usage();
        throw std::exception();
    }

    // get pid arguments
    int oldPid = -1;
    int newPid = -1;
    int pmtPid = -1;
    int fromPacket = -1;
    int toPacket = -1;

    std::vector<int> pmt_pid_list = {};

    // get arguments to oldPid and newPid;
    get_arguments(argc, argv, commandIdx, &oldPid, &newPid, &pmtPid, &fromPacket, &toPacket);

    // read input file
    FILE *infptr, *ofptr;
    infptr = fopen(input_file.c_str(), "rb");
    ofptr = fopen(output_file.c_str(), "wb");
    const int TS_BYTE_SIZE = 188;
    unsigned char ts_packet[TS_BYTE_SIZE];
//    fromPacket = (fromPacket == -1) ? 0 : fromPacket;
    int pcrPid = -1;
    int progNum = -1;
    if (infptr != NULL && ofptr != NULL)
    {
        int total_packet_count = -1;
        while (true)
        {
            unsigned char sync_byte = 0x47;
            unsigned char first_byte[1];
            int cnt = fread(first_byte, 1, 1, infptr);
            if (cnt != 1 || first_byte[0] != sync_byte)
            {
                printf("Finish reading ts file with total packet read: %d.\n", total_packet_count+1);
                break;
            }
            total_packet_count++;

            fseek(infptr, -1, SEEK_CUR);
            fread(ts_packet, sizeof(ts_packet), 1, infptr);

            // do not manipulate if not in range
            if ((fromPacket != -1 && total_packet_count < fromPacket) || (toPacket != -1 && total_packet_count > toPacket))
            {
                fwrite(ts_packet, sizeof(ts_packet), 1, ofptr);
                continue;
            }

            unsigned int pid = get_packet_pid(ts_packet);
            // find PAT packet to find corresponding PMT packet
            if (pmtPid == -1)
            {
                // skip until pmt pid is found;
                if (pmt_pid_list.size() > 0)
                {
                    for (int i : pmt_pid_list)
                    {
                        if (pid == i)
                        {
                            // check if current PMT packet contains oldPid
                            int payload_index = skip_adaptation_packet(ts_packet);
                            if (payload_index != -1)
                            {
                                if (modify_pmt_if_contain_oldpid(payload_index, ts_packet, oldPid, newPid))
                                {
                                    pmtPid = i;
                                    printf("Modifying PID starting from this PMT [%d] packet at packet number: %d\n", pmtPid, total_packet_count);
                                    fwrite(ts_packet, sizeof(ts_packet), 1, ofptr);
                                }
                            }
                        }
                    }
                }
                else if (pid == 0)
                {
                    printf("Finding PMT PID in current PAT packet: %d\n", total_packet_count);
                    int payload_index = skip_adaptation_packet(ts_packet);
                    if (payload_index != -1)
                    {
                        // modify Program map PID
                        get_pmt_pid_list(payload_index, ts_packet, pmt_pid_list);
                        printf("pmt pid list %d: ", pmt_pid_list.size());
                        for (int i : pmt_pid_list)
                        {
                            printf("%d, ", i);
                        }
                        printf("\n");
                    }
                }
            }
            // if PMT pid is found, change pid
            else
            {
                if (pid == pmtPid)
                {
                    // update element stream PID in PMT packet
                    int payload_index = skip_adaptation_packet(ts_packet);
                    if (payload_index != -1)
                    {
                        if (pcrPid == -1) {
                            int section_length_position_end = payload_index + 3;              // at 3rd-4th byte
                            pcrPid = (ts_packet[payload_index + 9] & 0x1F) << 8 | ts_packet[payload_index + 10];
                            printf("PCR pid is [%d]\n", pcrPid);
                        }
                        if (progNum == -1) {
                            progNum = ts_packet[payload_index + 4] << 8 | ts_packet[payload_index + 5];
                            printf("program number is [%d]\n", progNum);
                        }

                        modify_pmt_if_contain_oldpid(payload_index, ts_packet, oldPid, newPid);
                    }
                }
                // change PES packet if found PES PID
                else if (pid == oldPid)
                {
                    write_pid(ts_packet, newPid, 1);
                }

                // write only if PMT PID is found
                fwrite(ts_packet, sizeof(ts_packet), 1, ofptr);
            }
        }
    }
    fclose(infptr);
    fclose(ofptr);
}

void changePmtPid(char *argv[], int argc, int commandIdx, std::string const &input_file, std::string const &output_file)
{
    if (argc <= commandIdx + 3)
    {
        printf("Please input required arguments for 'changePmtPid'\n");
        print_usage();
        throw std::exception();
    }

    // get pid arguments
    int oldPid = -1;
    int newPid = -1;
    int fromPacket = -1;
    int toPacket = -1;
    // get arguments to oldPid and newPid;
    get_arguments(argc, argv, commandIdx, &oldPid, &newPid, nullptr, &fromPacket, &toPacket);
    // read input file
    FILE *infptr, *ofptr;
    infptr = fopen(input_file.c_str(), "rb");
    ofptr = fopen(output_file.c_str(), "wb");
    const int TS_BYTE_SIZE = 188;
    unsigned char ts_packet[TS_BYTE_SIZE];
    if (infptr != NULL && ofptr != NULL)
    {
        //* fseek can read the file at specific location
        //* without looping through the big file.
        //* Otherwise, it wastes a lot of memory.
        int total_packet_count = -1;
        // printf("The first seek_cur is %ld\n", ftell(infptr));
        while (true)
        {
            unsigned char sync_byte = 0x47;
            unsigned char first_byte[1];
            fread(first_byte, 1, 1, infptr);
            if (total_packet_count >= 20 || first_byte[0] != sync_byte)
            {
                printf("Finish reading ts file.\n");
                break;
            }
            total_packet_count++;

            // fseek: to set the read position
            // -1 to re-read the first_byte
            // start reading from the start of every packets
            fseek(infptr, -1, SEEK_CUR);
            fread(ts_packet, sizeof(ts_packet), 1, infptr);

            // do not manipulate if not in range
            if (fromPacket != -1 && total_packet_count < fromPacket && toPacket != -1 && total_packet_count > toPacket)
            {
                fwrite(ts_packet, sizeof(ts_packet), 1, ofptr);
                continue;
            }

            unsigned int pid = get_packet_pid(ts_packet);

            // change PMT pid inside PAT packet
            if (pid == 0)
            {
                // printf("Changing PAT packet\n");
                int payload_index = skip_adaptation_packet(ts_packet);
                // printf("payload_index %d\n", payload_index);
                if (payload_index != -1)
                {
                    // modify Program map PID
                    modify_pat_pmtpid(payload_index, ts_packet, oldPid, newPid);
                }
            }
            // change PMT pid if found PMT packet
            else if (pid == oldPid)
            {
                // printf("=== Found the PMT packet - the pid is %d\n", pid);
                write_pid(ts_packet, newPid, 1);
            }
            fwrite(ts_packet, sizeof(ts_packet), 1, ofptr);
        }
    }
    fclose(infptr);
    fclose(ofptr);
}

// normalize the arguments to lower case
void update_arguments_to_lower(int argc, char *argv[], int commandIdx)
{
    for (int i = commandIdx; i < argc; i++)
    {
        // printf("current command: %s\n", argv[i]);
        std::string value{argv[i]};
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c)
                       {
                           return std::tolower(c);
                       });
        strcpy(argv[i], value.c_str());
    }
}

void get_arguments(int argc, char *argv[], int commandIdx, int *const oldPid = NULL, int *const newPid = NULL, int *const pmtPid = NULL, int *const fromPacket = NULL, int *const toPacket = NULL)
{
    update_arguments_to_lower(argc, argv, commandIdx);
    while (commandIdx < argc)
    {
        // for (int i = 0; argv[i] != NULL; i++)
        // {
        //     printf("%s\n", argv[i]);
        // }
        // printf("The argc %d, commandIdx %d\n", argc, commandIdx);
        if (oldPid && *oldPid == -1 && strcmp(argv[commandIdx], "--oldpid") == 0)
        {
            *oldPid = std::stoi(std::string(argv[commandIdx + 1]));
            commandIdx += 2;
        }
        else if (newPid && *newPid == -1 && strcmp(argv[commandIdx], "--newpid") == 0)
        {
            *newPid = std::stoi(std::string(argv[commandIdx + 1]));
            commandIdx += 2;
        }
        else if (pmtPid && *pmtPid == -1 && strcmp(argv[commandIdx], "--pmtpid") == 0)
        {
            *pmtPid = std::stoi(std::string(argv[commandIdx + 1]));
            commandIdx += 2;
        }
        else if (fromPacket && *fromPacket == -1 && strcmp(argv[commandIdx], "--from") == 0)
        {
            *fromPacket = std::stoi(std::string(argv[commandIdx + 1]));
            commandIdx += 2;
        }
        else if (toPacket && *toPacket == -1 && strcmp(argv[commandIdx], "--to") == 0)
        {
            *toPacket = std::stoi(std::string(argv[commandIdx + 1]));
            commandIdx += 2;
        }
        else
        {
            printf("Argument does not match.\n");
            break;
        }
    }
    printf("Updating the old PID [%d] to new PID [%d].\n", *oldPid, *newPid);
}

void print_packet(unsigned const char packet[], int packet_size)
{
    //* when pass to char array to function, it cannot use sizeof()
    //* because it will degrade to pass pointer instead: print_packet(char *packet)
    //* so sizeof(char *) will not result an array size
    for (int i = 0; i < packet_size; i++)
    {
        printf("%X ", packet[i]);
    }
    printf("\n");
}

std::string get_filename()
{
    // this is the full path name
    std::string file = __FILE__;
    size_t index = file.find_last_of("/\\");
    return file.substr(index + 1);
}

void print_usage()
{
    std::string cur_file_name = get_filename();
    printf("%s - This file is for modifying TS source file\n", cur_file_name.c_str());
    printf("Usage: ./%s.exe <function> <input_filename> [--out <output_filename>] [options..]\n", cur_file_name.c_str());
    printf("\tfunction:\n");
    // changePid
    printf("\t\tchangePid - change the stream element PID from old to new PID, video/audio/data. If oldPid == Pcr pid, will also update to new pcr pid too.\n");
    printf("\t\t\toptions: --oldPid <oldPid> --newPid <newPid>\n");
    printf("\t\t\t(optional) --pmtPid <pmtPid> (if not specified, it will start modifying packets AFTER\n");
    printf("\t\t\t\t\tfinding the PMT packet that contains the element PID.)\n");
    printf("\t\t\t(optional) --from <packet_no> --to <packet_no>\n");

    // removePid
    printf("\t\tremovePid - remove the target stream element PID packets\n");
    printf("\t\t\toptions: <targetPid>\n");
    printf("\t\t\t(optional --from <packet_no> --to <packet_no>\n");

    // changePmtPid
    printf("\t\tchangePmtPid - change the Pmt pid to new pid\n");
    printf("\t\t\toptions: --oldPid <oldPid> --newPid <newPid>\n");
    printf("\t\t\t(optional) --from <packet_no> --to <packet_no>\n");
}

int main(int argc, char *argv[])
{
    int commandIdx = 0; // 0-based index. keep track on which argument has been read
    printf("Current c++ standard: %ld\n", __cplusplus);

    // check command
    commandIdx++;
    if (argc <= commandIdx)
    {
        printf("Please specify the function.\n");
        print_usage();
        return 1;
    }
    std::string input_file{};
    std::string output_file{};
    try
    {
        if (strcmp(argv[commandIdx], "changePid") == 0)
        {
            commandIdx = get_input_output_file(argv, argc, input_file, output_file);
            changePid(argv, argc, commandIdx, input_file, output_file);
        }
        else if (strcmp(argv[commandIdx], "removePid") == 0)
        {
            commandIdx = get_input_output_file(argv, argc, input_file, output_file);
        }
        else if (strcmp(argv[commandIdx], "changePmtPid") == 0)
        {
            commandIdx = get_input_output_file(argv, argc, input_file, output_file);
            changePmtPid(argv, argc, commandIdx, input_file, output_file);
        }
        else
        {
            printf("Please specify the function correctly.\n");
            print_usage();
            return 1;
        }
    }
    catch (std::exception ex)
    {
        return 1;
    }

    std::cout << "Finished\n";
    return 0;
}
