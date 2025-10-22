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
        static const std::vector<std::string> allowedExtensions = {".jpg", ".jpeg", ".png", ".gif", ".webp"};
        std::string ext = std::filesystem::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return std::find(allowedExtensions.begin(), allowedExtensions.end(), ext) != allowedExtensions.end();
    }

    // Generate unique filename using libuuid
    std::string generateUniqueFilename(const std::string &originalFilename) {
        uuid_t uuid;
        uuid_generate_random(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        return std::string(uuid_str) + std::filesystem::path(originalFilename).extension().string();
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
        return result.substr(0, 500);
    }

    // Create error response
    HttpResponsePtr createErrorResponse(const std::string &errorMsg, HttpStatusCode code) {
        Json::Value result;
        result["error"] = errorMsg;
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(code);
        return resp;
    }

    // Validate product fields
    bool validateProductFields(const std::string &title, const std::string &description, double price, int quantity,
                               std::string &errorMsg) {
        if (title.empty() || description.empty()) {
            errorMsg = "Title and description are required";
            return false;
        }
        if (price <= 0.0) {
            errorMsg = "Price must be positive";
            return false;
        }
        if (quantity < 0) {
            errorMsg = "Quantity must be non-negative";
            return false;
        }
        return true;
    }

    // Handle file upload
    std::optional<std::string> handleFileUpload(const std::vector<HttpFile> &files, std::string &errorMsg) {
        if (files.empty()) {
            LOG_DEBUG << "No file uploaded; proceeding without image";
            return std::nullopt;
        }

        const auto &file = files[0];
        std::string originalFilename = file.getFileName();
        LOG_DEBUG << "Uploaded file: " << originalFilename;

        if (!isValidImageExtension(originalFilename)) {
            errorMsg = "Invalid image file type. Allowed: jpg, jpeg, png, gif , webp";
            return std::nullopt;
        }

        if (file.fileLength() > 5 * 1024 * 1024) {
            errorMsg = "File size exceeds 5MB limit";
            return std::nullopt;
        }

        std::string filename = generateUniqueFilename(sanitizeFilename(originalFilename));
        std::string targetDir = "./Uploads/products/";
        std::filesystem::create_directories(targetDir);
        std::string targetPath = targetDir + filename;

        if (file.saveAs(targetPath) != 0) {
            errorMsg = "Failed to save uploaded file";
            return std::nullopt;
        }

        LOG_DEBUG << "File saved successfully: " << targetPath;
        return targetPath;
    }
}

void productsControllers::createProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    LOG_DEBUG << "Processing POST /api/products";
    LOG_DEBUG << "Content-Type: " << req->getHeader("Content-Type");
    LOG_DEBUG << "Request body size: " << req->getBody().length() << " bytes";
    LOG_TRACE << "Raw request body (first 500 chars): " << toPrintableString(req->getBody());

    try {
        // Get database client
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            callback(createErrorResponse("Database client not initialized", k500InternalServerError));
            return;
        }

        // Initialize product fields
        std::string title, description, imagePath;
        double price = 0.0;
        int quantity = 0;
        std::string errorMsg;
        std::optional<std::string> contentType = req->getHeader("Content-Type");

        // Handle request based on Content-Type
        if (contentType && contentType->find("multipart/form-data") != std::string::npos) {
            // Parse multipart form-data
            LOG_DEBUG << "Parsing multipart/form-data request";
            MultiPartParser parser;
            if (parser.parse(req) != 0) {
                LOG_ERROR << "Failed to parse multipart form data. Content-Type: " << *contentType;
                LOG_DEBUG << "Parser parameters count: " << parser.getParameters().size();
                LOG_DEBUG << "Parser files count: " << parser.getFiles().size();
                callback(createErrorResponse(
                    "Failed to parse multipart form data. Ensure all required fields (title, description, price, quantity) and a valid image file (if provided) are included correctly.",
                    k400BadRequest));
                return;
            }

            // Extract form fields
            const auto &parameters = parser.getParameters();
            LOG_DEBUG << "Form fields received: " << parameters.size();
            for (const auto &param: parameters) {
                LOG_DEBUG << "Form field: " << param.first << " = " << param.second;
            }

            title = parameters.find("title") != parameters.end() ? parameters.at("title") : "";
            description = parameters.find("description") != parameters.end() ? parameters.at("description") : "";
            auto priceStr = parameters.find("price") != parameters.end() ? parameters.at("price") : "";
            auto quantityStr = parameters.find("quantity") != parameters.end() ? parameters.at("quantity") : "";

            // Check for missing fields
            if (title.empty() || description.empty() || priceStr.empty() || quantityStr.empty()) {
                LOG_ERROR << "Missing required fields: "
                        << "title=" << (title.empty() ? "missing" : title) << ", "
                        << "description=" << (description.empty() ? "missing" : description) << ", "
                        << "price=" << (priceStr.empty() ? "missing" : priceStr) << ", "
                        << "quantity=" << (quantityStr.empty() ? "missing" : quantityStr);
                callback(createErrorResponse(
                    "All required fields (title, description, price, quantity) must be provided", k400BadRequest));
                return;
            }

            // Parse price and quantity
            try {
                price = std::stod(priceStr);
                quantity = std::stoi(quantityStr);
            } catch (const std::exception &e) {
                LOG_ERROR << "Invalid price or quantity format: " << e.what();
                callback(createErrorResponse("Invalid price or quantity format", k400BadRequest));
                return;
            }

            // Handle file upload
            if (auto image = handleFileUpload(parser.getFiles(), errorMsg)) {
                imagePath = *image;
            } else if (!errorMsg.empty()) {
                LOG_ERROR << errorMsg;
                callback(createErrorResponse(errorMsg, k400BadRequest));
                return;
            }
        } else {
            // Parse JSON body
            LOG_DEBUG << "Parsing JSON body";
            auto json = req->getJsonObject();
            if (!json || json->empty()) {
                LOG_ERROR << "Invalid or missing JSON body";
                callback(createErrorResponse("Invalid or missing JSON body", k400BadRequest));
                return;
            }

            if (!json->isMember("title") || !json->isMember("description") ||
                !json->isMember("price") || !json->isMember("quantity")) {
                LOG_ERROR << "Missing required JSON fields";
                callback(createErrorResponse("Missing required JSON fields: title, description, price, quantity",
                                             k400BadRequest));
                return;
            }

            title = (*json)["title"].asString();
            description = (*json)["description"].asString();
            price = (*json)["price"].asDouble();
            quantity = (*json)["quantity"].asInt();
            imagePath = json->isMember("image") ? (*json)["image"].asString() : "";
        }

        // Validate fields
        if (!validateProductFields(title, description, price, quantity, errorMsg)) {
            LOG_ERROR << errorMsg;
            callback(createErrorResponse(errorMsg, k400BadRequest));
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
                callback(createErrorResponse(fmt::format("Database error: {}", e.base().what()),
                                             k500InternalServerError));
            });
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        callback(createErrorResponse(fmt::format("Server error: {}", e.what()), k500InternalServerError));
    }
}

void productsControllers::updateProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    LOG_DEBUG << "Processing PUT /api/products/" << id;
    LOG_DEBUG << "Content-Type: " << req->getHeader("Content-Type");
    LOG_DEBUG << "Request body size: " << req->getBody().length() << " bytes";
    LOG_TRACE << "Raw request body (first 500 chars): " << toPrintableString(req->getBody());

    try {
        // Get database client
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            callback(createErrorResponse("Database client not initialized", k500InternalServerError));
            return;
        }

        // Initialize product fields
        std::string title, description, imagePath;
        double price = 0.0;
        int quantity = 0;
        std::string errorMsg;
        std::optional<std::string> contentType = req->getHeader("Content-Type");

        // Handle request based on Content-Type
        if (contentType && contentType->find("multipart/form-data") != std::string::npos) {
            // Parse multipart form-data
            LOG_DEBUG << "Parsing multipart/form-data request";
            MultiPartParser parser;
            if (parser.parse(req) != 0) {
                LOG_ERROR << "Failed to parse multipart form data. Content-Type: " << *contentType;
                LOG_DEBUG << "Parser parameters count: " << parser.getParameters().size();
                LOG_DEBUG << "Parser files count: " << parser.getFiles().size();
                callback(createErrorResponse(
                    "Failed to parse multipart form data. Ensure all required fields (title, description, price, quantity) and a valid image file (if provided) are included correctly.",
                    k400BadRequest));
                return;
            }

            // Extract form fields
            const auto &parameters = parser.getParameters();
            LOG_DEBUG << "Form fields received: " << parameters.size();
            for (const auto &param: parameters) {
                LOG_DEBUG << "Form field: " << param.first << " = " << param.second;
            }

            title = parameters.find("title") != parameters.end() ? parameters.at("title") : "";
            description = parameters.find("description") != parameters.end() ? parameters.at("description") : "";
            auto priceStr = parameters.find("price") != parameters.end() ? parameters.at("price") : "";
            auto quantityStr = parameters.find("quantity") != parameters.end() ? parameters.at("quantity") : "";

            // Check for missing fields
            if (title.empty() || description.empty() || priceStr.empty() || quantityStr.empty()) {
                LOG_ERROR << "Missing required fields: "
                        << "title=" << (title.empty() ? "missing" : title) << ", "
                        << "description=" << (description.empty() ? "missing" : description) << ", "
                        << "price=" << (priceStr.empty() ? "missing" : priceStr) << ", "
                        << "quantity=" << (quantityStr.empty() ? "missing" : quantityStr);
                callback(createErrorResponse(
                    "All required fields (title, description, price, quantity) must be provided", k400BadRequest));
                return;
            }

            // Parse price and quantity
            try {
                price = std::stod(priceStr);
                quantity = std::stoi(quantityStr);
            } catch (const std::exception &e) {
                LOG_ERROR << "Invalid price or quantity format: " << e.what();
                callback(createErrorResponse("Invalid price or quantity format", k400BadRequest));
                return;
            }

            // Handle file upload
            if (auto image = handleFileUpload(parser.getFiles(), errorMsg)) {
                imagePath = *image;
            } else if (!errorMsg.empty()) {
                LOG_ERROR << errorMsg;
                callback(createErrorResponse(errorMsg, k400BadRequest));
                return;
            }
        } else {
            // Parse JSON body
            LOG_DEBUG << "Parsing JSON body";
            auto json = req->getJsonObject();
            if (!json || json->empty()) {
                LOG_ERROR << "Invalid or missing JSON body";
                callback(createErrorResponse("Invalid or missing JSON body", k400BadRequest));
                return;
            }

            if (!json->isMember("title") || !json->isMember("description") ||
                !json->isMember("price") || !json->isMember("quantity")) {
                LOG_ERROR << "Missing required JSON fields";
                callback(createErrorResponse("Missing required JSON fields: title, description, price, quantity",
                                             k400BadRequest));
                return;
            }

            title = (*json)["title"].asString();
            description = (*json)["description"].asString();
            price = (*json)["price"].asDouble();
            quantity = (*json)["quantity"].asInt();
            imagePath = json->isMember("image") ? (*json)["image"].asString() : "";
        }

        // Validate fields
        if (!validateProductFields(title, description, price, quantity, errorMsg)) {
            LOG_ERROR << errorMsg;
            callback(createErrorResponse(errorMsg, k400BadRequest));
            return;
        }

        // Update product in database
        LOG_DEBUG << "Updating product in database: id=" << id;
        Mapper<Productcrud> mapper(client);
        auto product = mapper.findByPrimaryKey(id);
        product.setTitle(title);
        product.setDescription(description);
        product.setPrice(price);
        product.setQuantity(quantity);
        if (!imagePath.empty()) {
            product.setImage(imagePath);
        }

        mapper.update(
            product,
            [callback, title](size_t count) {
                Json::Value res;
                res["status"] = "success";
                res["message"] = fmt::format("Product with title '{}' updated successfully", title);
                LOG_INFO << "Product updated successfully";
                auto resp = HttpResponse::newHttpJsonResponse(res);
                resp->setStatusCode(k200OK);
                callback(resp);
            },
            [callback](const DrogonDbException &e) {
                LOG_ERROR << "Database error: " << e.base().what();
                callback(createErrorResponse(fmt::format("Database error: {}", e.base().what()),
                                             k500InternalServerError));
            });
    } catch (const orm::UnexpectedRows &e) {
        LOG_ERROR << "Product not found: " << e.what();
        callback(createErrorResponse("Product not found", k404NotFound));
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        callback(createErrorResponse(fmt::format("Server error: {}", e.what()), k500InternalServerError));
    }
}

void productsControllers::getAllProducts(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback) {
    try {
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            callback(createErrorResponse("Database client not initialized", k500InternalServerError));
            return;
        }

        Mapper<Productcrud> mapper(client);
        auto products = mapper.findAll();
        Json::Value data(Json::arrayValue);
        for (const auto &p: products) {
            data.append(p.toJson());
        }

        Json::Value result;
        result["status"] = "success";
        result["message"] = "List of products fetched successfully";
        result["data"] = data;
        auto resp = HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        callback(createErrorResponse(fmt::format("Server error: {}", e.what()), k500InternalServerError));
    }
}

void productsControllers::getProductById(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    try {
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            callback(createErrorResponse("Database client not initialized", k500InternalServerError));
            return;
        }

        Mapper<Productcrud> mapper(client);
        auto product = mapper.findByPrimaryKey(id);
        Json::Value res;
        res["status"] = "success";
        res["data"] = product.toJson();
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const orm::UnexpectedRows &e) {
        LOG_ERROR << "Product not found: " << e.what();
        callback(createErrorResponse("Product not found", k404NotFound));
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        callback(createErrorResponse(fmt::format("Server error: {}", e.what()), k500InternalServerError));
    }
}

void productsControllers::deleteProduct(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback, int id) {
    try {
        auto client = app().getDbClient();
        if (!client) {
            LOG_ERROR << "Database client not initialized";
            callback(createErrorResponse("Database client not initialized", k500InternalServerError));
            return;
        }

        Mapper<Productcrud> mapper(client);
        mapper.deleteByPrimaryKey(id);
        Json::Value res;
        res["status"] = "success";
        res["message"] = "Product deleted successfully";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const orm::UnexpectedRows &e) {
        LOG_ERROR << "Product not found: " << e.what();
        callback(createErrorResponse("Product not found", k404NotFound));
    } catch (const std::exception &e) {
        LOG_ERROR << "Server error: " << e.what();
        callback(createErrorResponse(fmt::format("Server error: {}", e.what()), k500InternalServerError));
    }
}
