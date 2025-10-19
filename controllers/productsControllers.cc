#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include "productsControllers.h"
#include <models/Productcrud.h>
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
#include <filesystem>
#include <fmt/core.h>
#include <uuid/uuid.h>
#include <string>
#include <fmt/format.h>
#include <optional>
#include <algorithm>
#include <cctype>
using namespace drogon;
using namespace drogon::orm;
using namespace std;
using namespace drogon_model::shopapi;

namespace {
    // Sanitize filename to prevent directory traversal
    std::string sanitizeFilename(const std::string &filename) {
        std::string sanitized = std::filesystem::path(filename).filename().string();
        sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
                                       [](char c) { return c == '/' || c == '\\' || c == ':'; }),
                        sanitized.end());
        return sanitized;
    }

    // Validate file extension
    bool isValidImageExtension(const std::string &filename) {
        static const std::vector<std::string> allowedExtensions = {".jpg", ".jpeg", ".png", ".gif"};
        std::string ext = std::filesystem::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return std::find(allowedExtensions.begin(), allowedExtensions.end(), ext) != allowedExtensions.end();
    }

    // Generate unique filename using libuuid
    std::string generateUniqueFilename(const std::string &originalFilename) {
        uuid_t uuid;
        uuid_generate_random(uuid);
        char uuid_str[37]; // UUID string is 36 chars + null terminator
        uuid_unparse(uuid, uuid_str);
        std::string ext = std::filesystem::path(originalFilename).extension().string();
        return std::string(uuid_str) + ext;
    }

    // Convert binary body to printable string for logging
    std::string toPrintableString(std::string_view body) {
        std::string result;
        for (char c: body) {
            if (std::isprint(static_cast<unsigned char>(c))) {
                result += c;
            } else {
                result += fmt::format("[{:02x}]", static_cast<unsigned char>(c));
            }
        }
        return result.substr(0, 500); // Limit to 500 chars for brevity
    }
}

void productsControllers::createProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    Json::Value result;
    LOG_DEBUG << "Processing POST /api/products";
    try {
        // Log request details
        LOG_DEBUG << "Content-Type: " << req->getHeader("Content-Type");
        LOG_DEBUG << "Request body size: " << req->getBody().length() << " bytes";
        LOG_TRACE << "Raw request body (first 500 chars): " << toPrintableString(req->getBody());

        // Get database client
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            result["error"] = "Database client not initialized";
            auto resp = HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        // Parse request data
        std::string title, description, imagePath;
        double price = 0.0;
        int quantity = 0;
        std::optional<std::string> contentType = req->getHeader("Content-Type");

        if (contentType && contentType->find("multipart/form-data") != std::string::npos) {
            // Handle multipart/form-data
            LOG_DEBUG << "Parsing multipart/form-data request";
            MultiPartParser parser;
            if (!parser.parse(req)) {
                LOG_ERROR << "Failed to parse multipart form data. Content-Type: " << *contentType;
                LOG_DEBUG << "Raw request body (first 500 chars): " << toPrintableString(req->getBody());

                // Additional logging to inspect parser state
                LOG_DEBUG << "Parser parameters count: " << parser.getParameters().size();
                LOG_DEBUG << "Parser files count: " << parser.getFiles().size();

                result["error"] =
                        "Failed to parse multipart form data. Ensure all required fields (title, description, price, quantity) and a valid image file (if provided) are included correctly.";
                auto resp = HttpResponse::newHttpJsonResponse(result);
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }

            // Log form fields
            const auto &parameters = parser.getParameters();
            LOG_DEBUG << "Form fields received: " << parameters.size();
            for (const auto &param: parameters) {
                LOG_DEBUG << "Form field: " << param.first << " = " << param.second;
            }

            // Extract form fields
            title = parameters.find("title") != parameters.end() ? parameters.at("title") : "";
            description = parameters.find("description") != parameters.end() ? parameters.at("description") : "";
            auto priceStr = parameters.find("price") != parameters.end() ? parameters.at("price") : "";
            auto quantityStr = parameters.find("quantity") != parameters.end() ? parameters.at("quantity") : "";

            // Check for missing fields
            if (title.empty() || description.empty() || priceStr.empty() || quantityStr.empty()) {
                LOG_ERROR << "Missing required fields in form data: "
                        << "title=" << (title.empty() ? "missing" : title) << ", "
                        << "description=" << (description.empty() ? "missing" : description) << ", "
                        << "price=" << (priceStr.empty() ? "missing" : priceStr) << ", "
                        << "quantity=" << (quantityStr.empty() ? "missing" : quantityStr);
                result["error"] = "All required fields (title, description, price, quantity) must be provided";
                auto resp = HttpResponse::newHttpJsonResponse(result);
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }

            // Parse price and quantity
            try {
                price = std::stod(priceStr);
                quantity = std::stoi(quantityStr);
            } catch (const std::exception &e) {
                LOG_ERROR << "Invalid price or quantity format: " << e.what();
                result["error"] = "Invalid price or quantity format";
                auto resp = HttpResponse::newHttpJsonResponse(result);
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }

            // Handle file upload
            const auto &files = parser.getFiles();
            LOG_DEBUG << "Number of files uploaded: " << files.size();
            if (!files.empty()) {
                const auto &file = files[0];
                std::string originalFilename = file.getFileName();
                LOG_DEBUG << "Uploaded file: " << originalFilename;

                if (!isValidImageExtension(originalFilename)) {
                    LOG_ERROR << "Invalid image file type: " << originalFilename;
                    result["error"] = "Invalid image file type. Allowed: jpg, jpeg, png, gif";
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    resp->setStatusCode(k400BadRequest);
                    callback(resp);
                    return;
                }

                // Check file size (max 5MB)
                if (file.fileLength() > 5 * 1024 * 1024) {
                    LOG_ERROR << "File size exceeds 5MB limit: " << file.fileLength();
                    result["error"] = "File size exceeds 5MB limit";
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    resp->setStatusCode(k400BadRequest);
                    callback(resp);
                    return;
                }

                std::string filename = generateUniqueFilename(sanitizeFilename(originalFilename));
                std::string targetDir = "./Uploads/products/";
                std::filesystem::create_directories(targetDir);
                std::string targetPath = targetDir + filename;

                // Save file and verify success
                if (file.saveAs(targetPath) != 0) {
                    LOG_ERROR << "Failed to save uploaded file: " << targetPath;
                    result["error"] = "Failed to save uploaded file";
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                    return;
                }
                LOG_DEBUG << "File saved successfully: " << targetPath;
                imagePath = targetPath;
            } else {
                LOG_DEBUG << "No file uploaded; proceeding without image";
            }
        } else {
            // Handle JSON body
            LOG_DEBUG << "Parsing JSON body";
            auto json = req->getJsonObject();
            if (!json || json->empty()) {
                LOG_ERROR << "Invalid or missing JSON body";
                result["error"] = "Invalid or missing JSON body";
                auto resp = HttpResponse::newHttpJsonResponse(result);
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }

            // Validate JSON fields
            if (!json->isMember("title") || !json->isMember("description") ||
                !json->isMember("price") || !json->isMember("quantity")) {
                LOG_ERROR << "Missing required JSON fields";
                result["error"] = "Missing required JSON fields: title, description, price, quantity";
                auto resp = HttpResponse::newHttpJsonResponse(result);
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }

            title = (*json)["title"].asString();
            description = (*json)["description"].asString();
            price = (*json)["price"].asDouble();
            quantity = (*json)["quantity"].asInt();
            imagePath = json->isMember("image") ? (*json)["image"].asString() : "";
        }

        // Validate required fields
        if (title.empty() || description.empty()) {
            LOG_ERROR << "Title or description is empty";
            result["error"] = "Title and description are required";
            auto resp = HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        // Validate price and quantity
        if (price <= 0.0 || quantity < 0) {
            LOG_ERROR << "Invalid price or quantity: price=" << price << ", quantity=" << quantity;
            result["error"] = "Price must be positive and quantity must be non-negative";
            auto resp = HttpResponse::newHttpJsonResponse(result);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        // Insert into database
        LOG_DEBUG << "Inserting product into database: title=" << title;
        Mapper<Productcrud> mapper(client);
        Productcrud product;
        product.setTitle(title);
        product.setDescription(description);
        product.setPrice(price);
        product.setQuantity(quantity);
        product.setImage(imagePath);
        mapper.insert(
            product,
            [callback](const Productcrud &p) {
                Json::Value res;
                res["status"] = "success";
                res["message"] = fmt::format("Product with title '{}' created successfully", p.getValueOfTitle());
                Json::Value data;
                data["id"] = p.getValueOfId();
                data["title"] = p.getValueOfTitle();
                data["description"] = p.getValueOfDescription();
                data["price"] = p.getValueOfPrice();
                data["quantity"] = p.getValueOfQuantity();
                data["image"] = p.getValueOfImage();
                res["data"] = data;
                LOG_INFO << "Product created successfully: id=" << p.getValueOfId();
                auto resp = HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(k201Created);
                callback(resp);
            },
            [callback](const DrogonDbException &e) {
                LOG_ERROR << "Database error: " << e.base().what();
                Json::Value res;
                res["error"] = fmt::format("Database error: {}", e.base().what());
                auto resp = HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            });
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        result["error"] = fmt::format("Server error: {}", e.what());
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void productsControllers::getAllProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    Json::Value result;
    try {
        // Connecting to database
        orm::Mapper<drogon_model::shopapi::Productcrud> mapper(app().getDbClient());
        // Get all products from DB
        auto products = mapper.findAll();
        // Convert results to JSON array
        Json::Value data(Json::arrayValue);
        for (auto &p: products) {
            data.append(p.toJson());
        }
        // Create success JSON response
        result["message"] = "List of products fetched successfully";
        result["status"] = "success";
        result["data"] = data;
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception &e) {
        // Handle DB or other errors
        result["status"] = "failed";
        result["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void productsControllers::getProductById(const HttpRequestPtr &ptr,
                                         std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    Json::Value res;
    try {
        orm::Mapper<drogon_model::shopapi::Productcrud> mapper(app().getDbClient());
        auto product = mapper.findByPrimaryKey(id);
        res["status"] = "success";
        res["data"] = product.toJson();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows &e) {
        // Handle "not found" case
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k404NotFound);
        callback(resp);
    } catch (const std::exception &e) {
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void productsControllers::updateProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    Json::Value res;
    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("title") || !(*json).isMember("description") || !(*json).isMember("price") ||
            !(*json).isMember("quantity")) {
            res["status"] = "failed";
            res["messages"] = "All fields required";
            auto resp = HttpResponse::newHttpJsonResponse(res);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        // Prepare DB mapper
        orm::Mapper<drogon_model::shopapi::Productcrud> mapper(app().getDbClient());
        // Find product existence
        auto product = mapper.findByPrimaryKey(id);
        // Update fields from JSON
        product.setTitle((*json)["title"].asString());
        product.setDescription((*json)["description"].asString());
        product.setImage((*json)["image"].asString());
        product.setPrice((*json)["price"].asDouble());
        product.setQuantity((*json)["quantity"].asInt());
        // Save changes to DB
        mapper.update(product);
        // Respond with success message
        res["status"] = "success";
        res["message"] = "Product with title updated successfully";
        res["data"] = product.toJson();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows &row) {
        res["status"] = "failed";
        res["message"] = row.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k404NotFound);
        callback(resp);
    } catch (const std::exception &e) {
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void productsControllers::deleteProduct(const HttpRequestPtr &ptr,
                                        std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    Json::Value res;
    try {
        orm::Mapper<drogon_model::shopapi::Productcrud> mapper(app().getDbClient());
        mapper.deleteByPrimaryKey(id);
        res["status"] = "success";
        res["message"] = "Product deleted successfully";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const drogon::orm::UnexpectedRows) {
        res["status"] = "error";
        res["message"] = "Product not found";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k404NotFound);
        callback(resp);
    } catch (const std::exception &e) {
        res["status"] = "failed";
        res["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
