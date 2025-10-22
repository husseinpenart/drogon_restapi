#include "userControllers.h"
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
#include <drogon/utils/Utilities.h>  // For hashing
#include <trantor/utils/Logger.h>
#include "Usercase.h"
#include <jwt-cpp/jwt.h>

void userControllers::Register(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid JSON");
        callback(resp);
        return;
    }

    std::string name = (*json)["name"].asString();
    std::string email = (*json)["email"].asString();
    std::string username = (*json)["username"].asString();
    std::string password = (*json)["password"].asString();

    if (name.empty() || email.empty() || username.empty() || password.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Missing required fields");
        callback(resp);
        return;
    }

    try {
        auto client = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::shopapi::Usercase> mapper(client);

        // Check for existing email or username
        auto existing = mapper.findBy(
            orm::Criteria(drogon_model::shopapi::Usercase::Cols::_email, orm::CompareOperator::EQ, email) ||
            orm::Criteria(drogon_model::shopapi::Usercase::Cols::_username, orm::CompareOperator::EQ, username));
        if (!existing.empty()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k409Conflict);
            resp->setBody("Email or username already exists");
            callback(resp);
            return;
        }

        // Generate UUID for id
        std::string userId = drogon::utils::getUuid();

        // Hash password
        std::string hashedPassword = drogon::utils::getSha256(password);

        // Create Usercase model instance
        drogon_model::shopapi::Usercase newUser;
        newUser.setId(userId); // Set UUID as id
        newUser.setName(name);
        newUser.setEmail(email);
        newUser.setUsername(username);
        newUser.setPassword(hashedPassword);

        // Insert into DB
        mapper.insert(newUser);

        // Return success with user ID
        Json::Value respJson;
        respJson["user_id"] = userId;
        auto resp = HttpResponse::newHttpJsonResponse(respJson);
        resp->setStatusCode(k201Created);
        resp->setBody("User registered successfully");
        callback(resp);
    } catch (const std::exception &e) {
        LOG_ERROR << "Register error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody(std::string("Server error: ") + e.what());
        callback(resp);
    }
}
// login
void userControllers::Login(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid JSON format");
        callback(resp);
        return;
    }

    std::string email = (*json)["email"].asString();
    std::string username = (*json)["username"].asString();
    std::string password = (*json)["password"].asString();

    if ((email.empty() && username.empty()) || password.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Missing email/username or password");
        callback(resp);
        return;
    }

    try {
        auto client = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::shopapi::Usercase> mapper(client);

        std::vector<drogon_model::shopapi::Usercase> users;
        if (!email.empty()) {
            users = mapper.findBy(orm::Criteria(drogon_model::shopapi::Usercase::Cols::_email, orm::CompareOperator::EQ,
                                                email));
        } else if (!username.empty()) {
            users = mapper.findBy(orm::Criteria(drogon_model::shopapi::Usercase::Cols::_username,
                                                orm::CompareOperator::EQ, username));
        }

        if (users.empty()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k401Unauthorized);
            resp->setBody("Invalid email/username or password");
            callback(resp);
            return;
        }

        drogon_model::shopapi::Usercase user = users[0];
        std::string hashedInput = drogon::utils::getSha256(password);

        if (hashedInput != user.getValueOfPassword()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k401Unauthorized);
            resp->setBody("Invalid email/username or password");
            callback(resp);
            return;
        }

        // Generate JWT with UUID
        auto token = jwt::create()
                .set_issuer("your_app")
                .set_subject(user.getValueOfId()) // UUID as string
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
                .sign(jwt::algorithm::hs256{"your-secret-key"});

        Json::Value respJson;
        respJson["token"] = token;
        respJson["user_id"] = user.getValueOfId();

        auto resp = HttpResponse::newHttpJsonResponse(respJson);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception &e) {
        LOG_ERROR << "Login error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody(std::string("Server error: ") + e.what());
        callback(resp);
    }
}


void userControllers::Profile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty()) {
        LOG_DEBUG << "No Authorization header provided";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Authorization header missing");
        callback(resp);
        return;
    }

    if (authHeader.find("Bearer ") != 0 || authHeader.length() <= 7) {
        LOG_DEBUG << "Invalid Authorization header: " << authHeader;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid or missing Bearer token");
        callback(resp);
        return;
    }

    std::string token = authHeader.substr(7);
    if (token.empty()) {
        LOG_DEBUG << "Token is empty after parsing";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Token is empty");
        callback(resp);
        return;
    }

    std::string userId;
    try {
        auto decoded = jwt::decode(token);
        jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{"your-secret-key"})
                .with_issuer("your_app")
                .verify(decoded);
        userId = decoded.get_subject(); // UUID as string
    } catch (const jwt::error::token_verification_error &e) {
        LOG_DEBUG << "Invalid JWT token: ";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid token");
        callback(resp);
        return;
    } catch (const std::exception &e) {
        LOG_DEBUG << "Token parsing error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Token is not a valid user ID");
        callback(resp);
        return;
    }

    try {
        auto client = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::shopapi::Usercase> mapper(client);
        drogon_model::shopapi::Usercase user = mapper.findByPrimaryKey(userId);

        Json::Value userJson;
        userJson["id"] = user.getValueOfId();
        userJson["name"] = user.getValueOfName();
        userJson["email"] = user.getValueOfEmail();
        userJson["username"] = user.getValueOfUsername();

        auto resp = HttpResponse::newHttpJsonResponse(userJson);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows &e) {
        LOG_DEBUG << "User not found for ID: " << userId;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        resp->setBody("User not found");
        callback(resp);
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody(std::string("Server error: ") + e.what());
        callback(resp);
    }
}

void userControllers::updateProfile(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty()) {
        LOG_DEBUG << "No Authorization header provided";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Authorization header missing");
        callback(resp);
        return;
    }

    if (authHeader.find("Bearer ") != 0 || authHeader.length() <= 7) {
        LOG_DEBUG << "Invalid Authorization header: " << authHeader;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid or missing Bearer token");
        callback(resp);
        return;
    }

    std::string token = authHeader.substr(7);
    if (token.empty()) {
        LOG_DEBUG << "Token is empty after parsing";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Token is empty");
        callback(resp);
        return;
    }

    std::string userId;
    try {
        auto decoded = jwt::decode(token);
        jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{"your-secret-key"}) // change and hide this in you real world api app :)
                .with_issuer("your_app")
                .verify(decoded);
        userId = decoded.get_subject(); // UUID as string
    } catch (const jwt::error::token_verification_error &e) {
        LOG_DEBUG << "Invalid JWT token: ";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid token");
        callback(resp);
        return;
    } catch (const std::exception &e) {
        LOG_DEBUG << "Token parsing error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Token is not a valid user ID");
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid JSON");
        callback(resp);
        return;
    }

    try {
        auto client = drogon::app().getDbClient();
        drogon::orm::Mapper<drogon_model::shopapi::Usercase> mapper(client);
        drogon_model::shopapi::Usercase user = mapper.findByPrimaryKey(userId);

        if (!(*json)["name"].empty()) user.setName((*json)["name"].asString());
        if (!(*json)["email"].empty()) user.setEmail((*json)["email"].asString());
        if (!(*json)["username"].empty()) user.setUsername((*json)["username"].asString());
        if (!(*json)["password"].empty()) {
            std::string newPass = (*json)["password"].asString();
            user.setPassword(drogon::utils::getSha256(newPass));
        }

        mapper.update(user);
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody("Profile updated");
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows &e) {
        LOG_DEBUG << "User not found for ID: " << userId;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        resp->setBody("User not found");
        callback(resp);
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody(std::string("Server error: ") + e.what());
        callback(resp);
    }
}
