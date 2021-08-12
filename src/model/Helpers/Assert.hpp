#ifndef ASSERT_HPP
#define ASSERT_HPP


using namespace std;

namespace Assert
{
    void AssertLong(bool condition, string file, unsigned int line, string message = "")
    {
        if (!condition)
        {
            string filename = file.substr(file.find_last_of("/\\") + 1);
            cout << "\n\033[1;31mASSERT in " << filename << " (ln" << line 
                << ") \033[0;31m" << message << "\033[0m" << endl;
            abort();
        }
    }
} // Assert

#endif