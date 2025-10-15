#include "SaveHandler.h"

#include <cinttypes>   // for PRIx32
#include <cstdint>     // for uint32_t
#include <cstdio>      // for sprintf, size_t

#include <cairo.h>                  // for cairo_surface_t
#include <gdk-pixbuf/gdk-pixbuf.h>  // for gdk_pixbuf_save
#include <glib.h>                   // for g_free, g_strdup_printf

#include "control/pagetype/PageTypeHandler.h"  // for PageTypeHandler
#include "control/xml/XmlAudioNode.h"          // for XmlAudioNode
#include "control/xml/XmlImageNode.h"          // for XmlImageNode
#include "control/xml/XmlNode.h"               // for XmlNode
#include "control/xml/XmlPointNode.h"          // for XmlPointNode
#include "control/xml/XmlTexNode.h"            // for XmlTexNode
#include "control/xml/XmlTextNode.h"           // for XmlTextNode
#include "model/AudioElement.h"                // for AudioElement
#include "model/BackgroundImage.h"             // for BackgroundImage
#include "model/Document.h"                    // for Document
#include "model/Element.h"                     // for Element, ELEMENT_IMAGE
#include "model/Font.h"                        // for XojFont
#include "model/Image.h"                       // for Image
#include "model/Layer.h"                       // for Layer
#include "model/LineStyle.h"                   // for LineStyle
#include "model/PageType.h"                    // for PageType
#include "model/Point.h"                       // for Point
#include "model/Stroke.h"                      // for Stroke, StrokeCapStyle
#include "model/StrokeStyle.h"                 // for StrokeStyle
#include "model/TexImage.h"                    // for TexImage
#include "model/Text.h"                        // for Text
#include "model/XojPage.h"                     // for XojPage
#include "pdf/base/XojPdfDocument.h"           // for XojPdfDocument
#include "util/OutputStream.h"                 // for GzOutputStream, Output...
#include "util/PathUtil.h"                     // for clearExtensions, normalizeAssetPath
#include "util/PlaceholderString.h"            // for PlaceholderString
#include "util/i18n.h"                         // for FS, _F

#include "config.h"  // for FILE_FORMAT_VERSION
#include "filesystem.h"

SaveHandler::SaveHandler() {
    this->firstPdfPageVisited = false;
    this->attachBgId = 1;
}

void SaveHandler::prepareSave(const Document* doc, const fs::path& target) {
    if (this->root) {
        // cleanup old data
        backgroundImages.clear();
    }

    this->firstPdfPageVisited = false;
    this->attachBgId = 1;

    root.reset(new XmlNode("xournal"));

    writeHeader();

    cairo_surface_t* preview = doc->getPreview();
    if (preview) {
        auto* image = new XmlImageNode("preview");
        image->setImage(preview);
        this->root->addChild(image);
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        p->getBackgroundImage().clearSaveState();
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        visitPage(root.get(), p, doc, static_cast<int>(i), target);
    }
}

void SaveHandler::writeHeader() {
    this->root->setAttrib("creator", PROJECT_STRING);
    this->root->setAttrib("fileversion", FILE_FORMAT_VERSION);
    this->root->addChild(new XmlTextNode("title", std::string{"Xournal++ document - see "} + PROJECT_HOMEPAGE_URL));
}

auto SaveHandler::getColorStr(Color c, unsigned char alpha) -> std::string {
    char str[10];
    sprintf(str, "#%08" PRIx32, uint32_t(c) << 8U | alpha);
    std::string color(str);
    return color;
}

void SaveHandler::writeTimestamp(XmlAudioNode* xmlAudioNode, const AudioElement* audioElement) {
    if (!audioElement->getAudioFilename().empty()) {
        /** set stroke timestamp value to the XmlPointNode */
        xmlAudioNode->setAttrib("ts", audioElement->getTimestamp());
        auto audioFilename = audioElement->getAudioFilename().generic_u8string();
        auto casted = char_cast(audioFilename);
        xmlAudioNode->setAttrib("fn", std::string{casted.begin(), casted.end()});
    }
}

void SaveHandler::visitStroke(XmlPointNode* stroke, const Stroke* s) {
    StrokeTool t = s->getToolType();

    unsigned char alpha = 0xff;

    if (t == StrokeTool::PEN) {
        stroke->setAttrib("tool", "pen");
        writeTimestamp(stroke, s);
    } else if (t == StrokeTool::ERASER) {
        stroke->setAttrib("tool", "eraser");
    } else if (t == StrokeTool::HIGHLIGHTER) {
        stroke->setAttrib("tool", "highlighter");
        alpha = 0x7f;
    } else {
        g_warning("Unknown StrokeTool::Value");
        stroke->setAttrib("tool", "pen");
    }

    stroke->setAttrib("color", getColorStr(s->getColor(), alpha).c_str());

    const auto& pts = s->getPointVector();

    stroke->setPoints(pts);

    if (s->hasPressure()) {
        std::vector<double> values;
        values.reserve(pts.size() + 1);
        values.emplace_back(s->getWidth());
        std::transform(pts.begin(), pts.end() - 1, std::back_inserter(values), [](const Point& p) { return p.z; });
        stroke->setAttrib("width", std::move(values));
    } else {
        stroke->setAttrib("width", s->getWidth());
    }

    visitStrokeExtended(stroke, s);
}

/**
 * Export the fill attributes
 */
void SaveHandler::visitStrokeExtended(XmlPointNode* stroke, const Stroke* s) {
    if (s->getFill() != -1) {
        stroke->setAttrib("fill", s->getFill());
    }

    const StrokeCapStyle capStyle = s->getStrokeCapStyle();
    if (capStyle == StrokeCapStyle::BUTT) {
        stroke->setAttrib("capStyle", "butt");
    } else if (capStyle == StrokeCapStyle::ROUND) {
        stroke->setAttrib("capStyle", "round");
    } else if (capStyle == StrokeCapStyle::SQUARE) {
        stroke->setAttrib("capStyle", "square");
    } else {
        g_warning("Unknown stroke cap type: %i", capStyle);
        stroke->setAttrib("capStyle", "round");
    }

    if (s->getLineStyle().hasDashes()) {
        stroke->setAttrib("style", StrokeStyle::formatStyle(s->getLineStyle()));
    }
}

void SaveHandler::visitLayer(XmlNode* page, const Layer* l) {
    auto* layer = new XmlNode("layer");
    page->addChild(layer);
    if (l->hasName()) {
        layer->setAttrib("name", l->getName().c_str());
    }

    for (const auto& e: l->getElementsView()) {
        if (e->getType() == ELEMENT_STROKE) {
            auto* s = dynamic_cast<const Stroke*>(e);
            auto* stroke = new XmlPointNode("stroke");
            layer->addChild(stroke);
            visitStroke(stroke, s);
        } else if (e->getType() == ELEMENT_TEXT) {
            const Text* t = dynamic_cast<const Text*>(e);
            auto* text = new XmlTextNode("text", t->getText());
            layer->addChild(text);

            const XojFont& f = t->getFont();

            text->setAttrib("font", f.getName().c_str());
            text->setAttrib("size", f.getSize());
            text->setAttrib("x", t->getX());
            text->setAttrib("y", t->getY());
            text->setAttrib("color", getColorStr(t->getColor()).c_str());

            writeTimestamp(text, t);
        } else if (e->getType() == ELEMENT_IMAGE) {
            auto* i = dynamic_cast<const Image*>(e);
            auto* image = new XmlImageNode("image");
            layer->addChild(image);

            image->setImage(i->getImage());

            image->setAttrib("left", i->getX());
            image->setAttrib("top", i->getY());
            image->setAttrib("right", i->getX() + i->getElementWidth());
            image->setAttrib("bottom", i->getY() + i->getElementHeight());
        } else if (e->getType() == ELEMENT_TEXIMAGE) {
            auto* i = dynamic_cast<const TexImage*>(e);
            auto* image = new XmlTexNode("teximage", std::string(i->getBinaryData()));
            layer->addChild(image);

            image->setAttrib("text", i->getText().c_str());
            image->setAttrib("left", i->getX());
            image->setAttrib("top", i->getY());
            image->setAttrib("right", i->getX() + i->getElementWidth());
            image->setAttrib("bottom", i->getY() + i->getElementHeight());
        }
    }
}

void SaveHandler::visitPage(XmlNode* root, ConstPageRef p, const Document* doc, int id, const fs::path& target) {
    auto* page = new XmlNode("page");
    root->addChild(page);
    page->setAttrib("width", p->getWidth());
    page->setAttrib("height", p->getHeight());

    auto* background = new XmlNode("background");
    page->addChild(background);

    writeBackgroundName(background, p);

    if (p->getBackgroundType().isPdfPage()) {
        /**
         * ATTENTION! The original xournal can only read the XML if the attributes are in the right order!
         * DO NOT CHANGE THE ORDER OF THE ATTRIBUTES!
         */

        background->setAttrib("type", "pdf");
        if (!firstPdfPageVisited) {
            firstPdfPageVisited = true;

            if (doc->isAttachPdf()) {
                background->setAttrib("domain", "attach");
                auto filepath = doc->getFilepath();
                Util::clearExtensions(filepath);
                filepath += ".xopp.bg.pdf";
                background->setAttrib("filename", "bg.pdf");

                GError* error = nullptr;
                if (!exists(filepath)) {
                    doc->getPdfDocument().save(filepath, &error);
                }

                if (error) {
                    if (!this->errorMessage.empty()) {
                        this->errorMessage += "\n";
                    }
                    this->errorMessage +=
                            FS(_F("Could not write background \"{1}\", {2}") % filepath.u8string() % error->message);

                    g_error_free(error);
                }
            } else {
                // "absolute" just means path. For backward compatibility, it is hard to change the word
                background->setAttrib("domain", "absolute");
                auto normalizedPath = Util::normalizeAssetPath(doc->getPdfFilepath(), target.parent_path(),
                                                               doc->getPathStorageMode());
                background->setAttrib("filename", char_cast(normalizedPath.c_str()));
            }
        }
        background->setAttrib("pageno", p->getPdfPageNr() + 1);
    } else if (p->getBackgroundType().isImagePage()) {
        background->setAttrib("type", "pixmap");

        int cloneId = p->getBackgroundImage().getCloneId();
        if (cloneId != -1) {
            background->setAttrib("domain", "clone");
            char* filename = g_strdup_printf("%i", cloneId);
            background->setAttrib("filename", filename);
            g_free(filename);
        } else if (p->getBackgroundImage().isAttached() && p->getBackgroundImage().getPixbuf()) {
            char* filename = g_strdup_printf("bg_%d.png", this->attachBgId++);
            background->setAttrib("domain", "attach");
            background->setAttrib("filename", filename);

            backgroundImages.emplace_back(p->getBackgroundImage());

            /*
             * Because BackgroundImage is basically a wrapped pointer, the following lines actually modify
             * *(p->getBackgroundImage().content) and thus the Document.
             * TODO Find a better way
             */
            backgroundImages.back().setFilepath(filename);
            backgroundImages.back().setCloneId(id);

            g_free(filename);
        } else {
            // "absolute" just means path. For backward compatibility, it is hard to change the word
            background->setAttrib("domain", "absolute");
            auto normalizedPath = Util::normalizeAssetPath(p->getBackgroundImage().getFilepath(), target.parent_path(),
                                                           doc->getPathStorageMode());
            background->setAttrib("filename", char_cast(normalizedPath.c_str()));

            BackgroundImage img = p->getBackgroundImage();

            /*
             * Because BackgroundImage is basically a wrapped pointer, the following line actually modifies
             * *(p->getBackgroundImage().content) and thus the Document.
             * TODO Find a better way
             */
            img.setCloneId(id);
        }
    } else {
        writeSolidBackground(background, p);
    }

    // no layer, but we need to write one layer, else the old Xournal cannot read the file
    if (p->getLayerCount() == 0) {
        auto* layer = new XmlNode("layer");
        page->addChild(layer);
    }

    for (const Layer* l: p->getLayersView()) {
        visitLayer(page, l);
    }
}

void SaveHandler::writeSolidBackground(XmlNode* background, ConstPageRef p) {
    background->setAttrib("type", "solid");
    background->setAttrib("color", getColorStr(p->getBackgroundColor()));
    background->setAttrib("style", PageTypeHandler::getStringForPageTypeFormat(p->getBackgroundType().format));

    // Not compatible with Xournal, so the background needs
    // to be changed to a basic one!
    if (!p->getBackgroundType().config.empty()) {
        background->setAttrib("config", p->getBackgroundType().config);
    }
}

void SaveHandler::writeBackgroundName(XmlNode* background, ConstPageRef p) {
    if (p->backgroundHasName()) {
        background->setAttrib("name", p->getBackgroundName());
    }
}

void SaveHandler::saveTo(const fs::path& filepath, ProgressListener* listener) {
    GzOutputStream out(filepath);

    if (!out.getLastError().empty()) {
        this->errorMessage = out.getLastError();
        return;
    }

    saveTo(&out, filepath, listener);

    out.close();

    if (this->errorMessage.empty()) {
        this->errorMessage = out.getLastError();
    }
}

/*

// =================================================================================
// METODO DI SALVATAGGIO PRINCIPALE (Orchestratore)
// =================================================================================
bool SaveHandler::save(Document* doc, const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        // Il file non esiste, dobbiamo crearlo da zero.
        return fullSave(doc, filepath);
    } else {
        // Il file esiste, procediamo con l'aggiornamento incrementale.
        return incrementalSave(doc, filepath);
    }
}

// =================================================================================
// SALVATAGGIO COMPLETO (per file nuovi)
// =================================================================================
bool SaveHandler::fullSave(Document* doc, const std::string& filepath) {
    pugi::xml_document xmlDoc;

    // Crea il nodo radice <xournal>
    auto rootNode = xmlDoc.append_child("xournal");
    // Aggiungi qui eventuali attributi al nodo radice (es. versione)
    rootNode.append_attribute("creator").set_value("Xournal++");
    
    // Itera su TUTTE le pagine del modello e le serializza
    for (const auto& pagePtr : doc->getPages()) {
        pugi::xml_node pageNode = serializePageToXmlNode(xmlDoc, pagePtr.get());
        rootNode.append_copy(pageNode);
    }

    // Salva il documento XML su file
    bool success = xmlDoc.save_file(filepath.c_str(), PUGIXML_TEXT("  "), pugi::format_default, pugi::encoding_utf8);

    if (success) {
        // Se il salvataggio è andato a buon fine, tutte le pagine sono "pulite"
        doc->clearAllDirtyFlags();
    }

    return success;
}

// =================================================================================
// SALVATAGGIO INCREMENTALE (per file esistenti)
// =================================================================================
bool SaveHandler::incrementalSave(Document* doc, const std::string& filepath) {
    pugi::xml_document xmlDoc;
    pugi::xml_parse_result result = xmlDoc.load_file(filepath.c_str());

    // Controlla se il file XML esistente è valido
    if (!result) {
        // Errore di parsing: il file potrebbe essere corrotto.
        // Per sicurezza, esegui un backup e poi un salvataggio completo.
        // (Logica di backup omessa per brevità)
        return fullSave(doc, filepath);
    }

    auto rootNode = xmlDoc.child("xournal");
    if (!rootNode) {
        // Il file XML non ha un nodo radice valido.
        return fullSave(doc, filepath);
    }

    bool modified = false;

    // Itera sulle pagine del MODELLO DATI
    for (const auto& pagePtr : doc->getPages()) {
        if (pagePtr->isDirty()) {
            modified = true;
            XojPage* page = pagePtr.get();

            // 1. Cerca il nodo esistente nel DOM tramite query XPath sull'UID
            std::string query = "/xournal/page[@uid='" + page->getUid() + "']";
            pugi::xpath_node xpathNode = xmlDoc.select_node(query.c_str());
            pugi::xml_node oldPageNode = xpathNode.node();

            // 2. Serializza la pagina "sporca" in un nuovo nodo
            pugi::xml_node newPageNode = serializePageToXmlNode(xmlDoc, page);

            if (oldPageNode) {
                // Il nodo esiste: sostituiscilo con quello nuovo
                rootNode.insert_copy_after(newPageNode, oldPageNode);
                rootNode.remove_child(oldPageNode);
            } else {
                // Il nodo non esiste: è una pagina nuova, quindi accodala
                rootNode.append_copy(newPageNode);
            }
        }
    }
    
    // **(Opzionale ma raccomandato) Gestione Pagine Eliminate**
    // Se hai una lista di UID eliminati:
    for (const std::string& deletedUid : doc->getDeletedPageUids()) {
        std::string query = "/xournal/page[@uid='" + deletedUid + "']";
        pugi::xml_node nodeToRemove = xmlDoc.select_node(query.c_str()).node();
        if (nodeToRemove) {
            rootNode.remove_child(nodeToRemove);
            modified = true;
        }
    }

    if (!modified) {
        // Nessuna pagina era "sporca", non c'è nulla da salvare.
        return true;
    }

    // Salva le modifiche sul file
    bool success = xmlDoc.save_file(filepath.c_str(), PUGIXML_TEXT("  "), pugi::format_default, pugi::encoding_utf8);
    
    if (success) {
        // Se il salvataggio è andato a buon fine, pulisci i flag delle pagine
        doc->clearAllDirtyFlags();
        // doc->clearDeletedPageUids(); // Se hai implementato la cancellazione
    }

    return success;
}


// =================================================================================
// HELPER PER LA SERIALIZZAZIONE DI UNA PAGINA
// =================================================================================
pugi::xml_node SaveHandler::serializePageToXmlNode(pugi::xml_document& doc, XojPage* page) {
    // Crea un nodo <page> temporaneo (non ancora attaccato al DOM principale)
    pugi::xml_node pageNode = doc.append_child("page"); // Aggiunto temporaneamente, verrà copiato

    // **QUI VA LA TUA LOGICA DI SERIALIZZAZIONE ESISTENTE, ADATTATA PER PUGIXML**
    // Invece di `XmlNode`, userai i metodi di `pugi::xml_node`.
    
    // 1. Aggiungi attributi alla pagina
    pageNode.append_attribute("uid").set_value(page->getUid().c_str());
    pageNode.append_attribute("width").set_value(std::to_string(page->getWidth()).c_str());
    pageNode.append_attribute("height").set_value(std::to_string(page->getHeight()).c_str());
    // ... altri attributi come background, etc.

    // 2. Itera sui layer della pagina
    for (const auto& layerPtr : page->getLayers()) {
        pugi::xml_node layerNode = pageNode.append_child("layer");
        // ... serializza gli elementi del layer (stroke, text, image) ...
        
        for (const auto& elementPtr : layerPtr->getElements()) {
             // Esempio per uno Stroke
            if (auto stroke = dynamic_cast<Stroke*>(elementPtr.get())) {
                pugi::xml_node strokeNode = layerNode.append_child("stroke");
                strokeNode.append_attribute("tool").set_value(stroke->getToolType().c_str());
                strokeNode.append_attribute("color").set_value(stroke->getColorAsString().c_str());

                // Serializza i dati dei punti
                std::string data = stroke->getDataAsString(); // Ipotizzando un metodo che formatta i punti
                strokeNode.text().set(data.c_str());
            }
            // ... gestisci altri tipi di elementi (Text, Image, etc.) ...
        }
    }
    
    // Rimuovi il nodo temporaneo dalla radice e restituiscilo
    // Sarà poi copiato nel documento di destinazione dalla funzione chiamante
    doc.remove_child(pageNode);
    return pageNode;
}

*/

void SaveHandler::saveTo(OutputStream* out, const fs::path& filepath, ProgressListener* listener) {
    // XMLNode should be locale-safe ( store doubles using Locale 'C' format

    out->write("<?xml version=\"1.0\" standalone=\"no\"?>\n");
    root->writeOut(out, listener);

    for (const BackgroundImage& img: backgroundImages) {
        auto tmpfn = (fs::path(filepath) += ".") += img.getFilepath();
        // Are we certain that does not modify the GdkPixbuf?
        if (!gdk_pixbuf_save(const_cast<GdkPixbuf*>(img.getPixbuf()), char_cast(tmpfn.u8string().c_str()), "png",
                             nullptr, nullptr)) {
            if (!this->errorMessage.empty()) {
                this->errorMessage += "\n";
            }

            this->errorMessage += FS(_F("Could not write background \"{1}\". Continuing anyway.") % tmpfn.u8string());
        }
    }
}

auto SaveHandler::getErrorMessage() -> const std::string& { return this->errorMessage; }
