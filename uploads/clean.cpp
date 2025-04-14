

#include "../http.h"

int main()
{
    std::vector<char> file_with_boundary = read_file_with_boundary("1.pdf");
        
                                    std::vector<char> clean_file = remove_boundary(file_with_boundary, "------WebKitFormBoundary6uaf3HKEIUSbUHOS");
                                    
                                    std::ofstream out("1.pdf", std::ios::binary);
                                    out.write(clean_file.data(), clean_file.size());
                                    out.close();
}