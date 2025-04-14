#include <zlib.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

bool gzipCompressFile(const std::string& filename) {
    // 读取原始文件
    std::ifstream in_file(filename, std::ios::binary | std::ios::ate);
    if (!in_file) {
        std::cerr << "Error opening input file: " << filename << std::endl;
        return false;
    }
    
    const size_t file_size = in_file.tellg();
    in_file.seekg(0);
    
    std::vector<char> input(file_size);
    if (!in_file.read(input.data(), file_size)) {
        std::cerr << "Error reading input file" << std::endl;
        return false;
    }
    in_file.close();

    // 准备压缩
    z_stream strm = {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                   16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "Compression initialization failed" << std::endl;
        return false;
    }

    // 动态输出缓冲区
    size_t out_capacity = compressBound(file_size);
    std::vector<char> output(out_capacity);
    
    strm.next_in = reinterpret_cast<Bytef*>(input.data());
    strm.avail_in = input.size();
    strm.next_out = reinterpret_cast<Bytef*>(output.data());
    strm.avail_out = output.size();

    int ret = Z_OK;
    do {
        ret = deflate(&strm, Z_FINISH);
        
        // 处理缓冲区扩容
        if (ret == Z_BUF_ERROR || strm.avail_out == 0) {
            const size_t old_size = output.size();
            output.resize(old_size * 2);
            strm.next_out = reinterpret_cast<Bytef*>(output.data() + old_size);
            strm.avail_out = output.size() - old_size;
        }
    } while (ret == Z_BUF_ERROR || (ret == Z_OK && strm.avail_out == 0));

    // 验证压缩结果
    if (ret != Z_STREAM_END) {
        std::cerr << "Compression failed: " << ret << std::endl;
        deflateEnd(&strm);
        return false;
    }

    // 写入压缩文件
    const std::string out_filename = filename + ".gz";
    std::ofstream out_file(out_filename, std::ios::binary);
    if (!out_file.write(output.data(), strm.total_out)) {
        std::cerr << "Error writing output file" << std::endl;
        deflateEnd(&strm);
        return false;
    }
    
    deflateEnd(&strm);
    std::cout << "Successfully compressed to: " << out_filename 
              << " (" << strm.total_out << " bytes)" << std::endl;
    return true;
}

// 使用示例
int main() {
    if (gzipCompressFile("test.txt")) {
        std::cout << "Compression successful" << std::endl;
    } else {
        std::cout << "Compression failed" << std::endl;
    }
    return 0;
}