#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <openssl/hmac.h>
#include <openssl/sha.h>

std::string hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digest_len;

    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         digest, &digest_len);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }

    return oss.str();
}

const std::string secret_key = "YOUR_SECRET_KEY";

std::string generateSignedCookie(const std::string& username) {
    time_t expire_time = time(nullptr) + 604800; // 7 天后过期
    std::string data = "user=" + username + "&expires=" + std::to_string(expire_time);
    std::string sign = hmac_sha256(data, secret_key);
    
    // 只对数据部分签名，返回的 cookie 值包含数据和签名
    std::string cookie_value = data + "&sign=" + sign;
    
    // HTTP 属性是 Set-Cookie 头的一部分，不是 cookie 值本身
    return cookie_value + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800";
}

bool verifySignedCookie(const std::string& cookie) {
    // 需要先去除 HTTP 属性部分（如果有）
    size_t attr_pos = cookie.find(";");
    std::string cookie_value = (attr_pos == std::string::npos) ? cookie : cookie.substr(0, attr_pos);
    
    size_t sign_pos = cookie_value.find("&sign=");
    if (sign_pos == std::string::npos) {
        return false;
    }
    std::string data = cookie_value.substr(0, sign_pos);
    std::string provided_sign = cookie_value.substr(sign_pos + 6);
    
    std::string calculated_sign = hmac_sha256(data, secret_key);
    return calculated_sign == provided_sign;
}

int main() {
    std::string cookieValue = generateSignedCookie("test");
    std::cout << "Set-Cookie: " << cookieValue << "\n";

    std::string cookieValue1 = generateSignedCookie("admin");
    std::cout << "Set-Cookie1: " << cookieValue1 << "\n";
    
    bool isValid = verifySignedCookie(cookieValue);
    std::cout << "Cookie is valid: " << (isValid? "Yes" : "No") << std::endl;

    bool isValid1 = verifySignedCookie(cookieValue1);
    std::cout << "Cookie1 is valid: " << (isValid1? "Yes" : "No") << std::endl;
    
    return 0;
}