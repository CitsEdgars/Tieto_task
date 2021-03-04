#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

using namespace std;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

void init_logging()
{
    time_t now = time(0);
    tm* localTime = localtime(&now);
    stringstream ss;
    ss << setw(2) << setfill('0') << localTime->tm_year;
    ss << setw(2) << setfill('0') << localTime->tm_mon + 1;
    ss << setw(2) << setfill('0') << localTime->tm_mday;

    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");

    logging::add_file_log(
        keywords::file_name = "Tieto_task_" + ss.str().substr(1, 6) + ".log",
        keywords::format = "[%TimeStamp%] [%Severity%] %Message%"
    );

    logging::add_common_attributes();
}

struct node
{
    string transType;
    string PANNumber;
    int amntProc;
    int tm_year;
    int tm_mon;
    int tm_day;
    int tm_hrs;
    int tm_mins;
    int tm_secs;
    string currencyName;
    node* next;
};

class LinkedList
{
private:
    const int tmStringPos[6] = { 0, 4, 6, 8, 10, 12 };
    const int tmStringLen[6] = { 4, 2, 2, 2, 2, 2 };

    const string transCodes[2] = { "00", "01" };
    const string transName[2] = { "Purchase", "Withdrawal" };

    const string currCodes[4] = { "840", "978", "826", "643" };
    const string currNames[4] = { "USD", "EUR", "GBP", "RUB" };

public:
    node* head, * tail;

    LinkedList() {
        head = NULL;
        tail = NULL;
    }

    void addNode(string transType, string PANNumber, string amountProcessed, string timeProcessed, string currencyCode) {
        node* temp = new node;
        BOOST_LOG_TRIVIAL(trace) << "Processing input data.";

        //TRANSACTION TYPE
        for (int i = 0; i < ((sizeof(transCodes) / sizeof(*transCodes))); i++) {
            if (!transType.compare(transCodes[i])) {
                temp->transType = transName[i];
                break;
            }
            if (i == sizeof(transCodes) / sizeof(*transCodes) - 1) BOOST_LOG_TRIVIAL(error) << "Incorrect transaction type data.";
        }

        //PAN NUMBER
        temp->PANNumber = PANNumber;

        //AMOUNT PROCESSED
        while (amountProcessed.find("0") == 0) {
            amountProcessed = amountProcessed.substr(1);
        }
        temp->amntProc = stoi(amountProcessed);

        //TIME PROCESSED
        temp->tm_year = stoi(timeProcessed.substr(tmStringPos[0], tmStringLen[0]));
        temp->tm_mon = stoi(timeProcessed.substr(tmStringPos[1], tmStringLen[1]));
        temp->tm_day = stoi(timeProcessed.substr(tmStringPos[2], tmStringLen[2]));
        temp->tm_hrs = stoi(timeProcessed.substr(tmStringPos[3], tmStringLen[3]));
        temp->tm_mins = stoi(timeProcessed.substr(tmStringPos[4], tmStringLen[4]));
        temp->tm_secs = stoi(timeProcessed.substr(tmStringPos[5], tmStringLen[5]));

        //CURRENCY CODE
        for (int i = 0; i < (sizeof(currCodes) / sizeof(*currCodes)); i++) {
            if (!currencyCode.compare(currCodes[i])) {
                temp->currencyName = currNames[i];
                break;
            }
            if (i == sizeof(currCodes) / sizeof(*currCodes) - 1) BOOST_LOG_TRIVIAL(error) << "Incorrect transaction currency data.";
        }
        temp->next = NULL;

        if (head == NULL){
            head = temp;
            tail = temp;
        }
        else{
            tail->next = temp;
            tail = tail->next;
        }
    }
};

int main(int argc, char** argv) {

    const int codeParts[5] = { 2, 16, 12, 14, 3 };
    const int codePos[5] = { 0, 2, 18, 30, 44 };
    const int expectedLineLength = 48;

    time_t now = time(0);
    tm* localTime = localtime(&now);

    init_logging();

    //INPUT
    string line;
    LinkedList transactionList;
    BOOST_LOG_TRIVIAL(trace) << "Opening input file.";
    ifstream InputFile(argv[1]);
    while (getline(InputFile, line)) {
        if ((line.length() + 1) != expectedLineLength) {
            BOOST_LOG_TRIVIAL(error) << "Incorrect input Data";
            continue;
        }
        string transType = line.substr(codePos[0], codeParts[0]);
        string PANNumber = line.substr(codePos[1], codeParts[1]);
        string amntProc = line.substr(codePos[2], codeParts[2]);
        string timeProc = line.substr(codePos[3], codeParts[3]);
        string currCode = line.substr(codePos[4], codeParts[4]);
        transactionList.addNode(transType, PANNumber, amntProc, timeProc, currCode);        
    }
    BOOST_LOG_TRIVIAL(trace) << "Input file read.";
    InputFile.close();
    
    //OUTPUT
    ofstream OutputFile(argv[2]);
    BOOST_LOG_TRIVIAL(trace) << "Creating output file."; 
    int transactionCounter = 0;
    int transactionSum = 0;

    OutputFile << "<root>\n";
    OutputFile << "<msg-list>\n";

    node* tmp;
    tmp = transactionList.head;
    while (tmp != NULL){
        OutputFile << "\t<msg>";

        transactionSum = transactionSum + tmp->amntProc;
        transactionCounter++;
        BOOST_LOG_TRIVIAL(trace) << "Processing transaction #" << transactionCounter << ".";

        string maskedPANNumber = tmp->PANNumber;
        maskedPANNumber.replace(6, 6, "******");        

        if (tmp->tm_mon > 12 || tmp->tm_day > 31 || tmp->tm_year > localTime->tm_year + 1900) {
            //SOME BETTER VALIDATION THAT CHECKS THE VIABILITY OF THE DATE
            BOOST_LOG_TRIVIAL(error) << "Incorrect transaction date data.";
            tmp = tmp->next;
            continue;
        } 

        BOOST_LOG_TRIVIAL(trace) << "Outputing to file.";
        OutputFile << tmp->transType <<
            " with card " << maskedPANNumber <<
            " on " << setw(2) << setfill('0') << tmp->tm_mon <<
            "." << setw(2) << setfill('0') << tmp->tm_day <<
            "." << tmp->tm_year <<
            " " << setw(2) << setfill('0') << tmp->tm_hrs <<
            ":" << setw(2) << setfill('0') << tmp->tm_mins <<
            ", amount " << tmp->amntProc/100.0 <<
            " " << tmp->currencyName << ".";
        tmp = tmp->next;
        OutputFile << "</msg>\n";
    }
    OutputFile << "</msg-list>\n";
    BOOST_LOG_TRIVIAL(trace) << "Transaction output completed.";
    OutputFile << "<totals cnt=\"" << transactionCounter << "\" "
        << "sum=\"" << transactionSum << "\" "
        << "date=\"" << 1900 + localTime->tm_year << "." 
        << setw(2) << setfill('0') << 1 + localTime->tm_mon << "."
        << setw(2) << setfill('0') << localTime->tm_mday << " "
        << setw(2) << setfill('0') << localTime->tm_hour << ":"
        << setw(2) << setfill('0') << localTime->tm_min << "\">\n";
    OutputFile << "</root>";
    OutputFile.close();
    BOOST_LOG_TRIVIAL(trace) << "Conversion completed.";
    return 0;
}