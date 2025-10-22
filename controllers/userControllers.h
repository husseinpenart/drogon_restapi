#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class userControllers : public drogon::HttpController<userControllers> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(userControllers::Register, "/api/register", Post);
        ADD_METHOD_TO(userControllers::Login, "/api/login", Post);
        ADD_METHOD_TO(userControllers::Profile, "/api/profile", Get);
        ADD_METHOD_TO(userControllers::updateProfile, "/api/update-profile", Put);
    METHOD_LIST_END

    static void Register(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);

    static void Login(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);

    static void Profile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    //
    static void updateProfile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
