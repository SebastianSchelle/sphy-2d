#ifndef TEST_SERVER_HPP
#define TEST_SERVER_HPP

#include <client.hpp>

class TestClient : public sphyc::Client {
  public:
    TestClient();
    ~TestClient();
};

#endif