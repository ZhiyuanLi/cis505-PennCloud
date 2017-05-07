#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
using namespace std;

int main()
{
    std::time_t t = std::time(nullptr);
    ostringstream curr_time;
    curr_time << std::put_time(std::localtime(&t), "%c %Z");
    string time = curr_time.str();
    cout<<time<<endl;
}
