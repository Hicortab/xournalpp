#include "SaveJob.h"

#include <memory>
#include <glib.h>

#include "control/Control.h"
#include "control/jobs/BlockingJob.h"
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/XojMsgBox.h"
#include "util/i18n.h"
#include "view/DocumentView.h"
#include "filesystem.h"

// Correzione: Includi l'header completo per XojPage
#include "model/XojPage.h"
#include "model/PageRef.h"
#include "pdf/base/XojPdfPage.h"

// Correzione: Includi tutti i nuovi header necessari
#include "control/xojfile/AbstractSaveHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "control/sqlite/SqliteSaveHandler.h"
#include "control/sqlite/FileFormat.h"

SaveJob::SaveJob(Control* control, std::function<void(bool)> callback):
        BlockingJob(control, _("Save")), callback(std::move(callback)) {}

SaveJob::~SaveJob() = default;

void SaveJob::run() {
    save();

    if (this->control->getWindow()) {
        callAfterRun();
    }
}

void SaveJob::afterRun() {
    if (!this->lastError.empty()) {
        XojMsgBox::showErrorToUser(control->getGtkWindow(), this->lastError);
        callback(false);
    } else {
        this->control->resetSavedStatus();
        callback(true);
    }
}

void SaveJob::updatePreview(Control* control) {
    const int previewSize = 128;

    Document* doc = control->getDocument();

    doc->lock();

    if (doc->getPageCount() > 0) {
        PageRef page = doc->getPage(0);

        double width = page->getWidth();
        double height = page->getHeight();

        double zoom = 1;

        if (width < height) {
            zoom = previewSize / height;
        } else {
            zoom = previewSize / width;
        }
        width *= zoom;
        height *= zoom;

        cairo_surface_t* crBuffer =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ceil_cast<int>(width), ceil_cast<int>(height));

        cairo_t* cr = cairo_create(crBuffer);
        cairo_scale(cr, zoom, zoom);

        xoj::view::BackgroundFlags flags = xoj::view::BACKGROUND_SHOW_ALL;

        // We don't have access to a PdfCache on which DocumentView relies for PDF backgrounds.
        // We thus print the PDF background by hand.
        if (page->getBackgroundType().isPdfPage()) {
            auto pgNo = page->getPdfPageNr();
            XojPdfPageSPtr popplerPage = doc->getPdfPage(pgNo);
            if (popplerPage) {
                popplerPage->render(cr);
            }
            flags.showPDF = xoj::view::HIDE_PDF_BACKGROUND;  // Already printed (if any)
        } else {
            flags.forceBackgroundColor = xoj::view::FORCE_AT_LEAST_BACKGROUND_COLOR;
        }

        DocumentView view;
        view.drawPage(page, cr, true /* don't render erasable */, flags);
        cairo_destroy(cr);
        doc->setPreview(crBuffer);
        cairo_surface_destroy(crBuffer);
    } else {
        doc->setPreview(nullptr);
    }

    doc->unlock();
}

auto SaveJob::save() -> bool {
    updatePreview(control);
    Document* doc = this->control->getDocument();
    
    fs::path target = doc->getFilepath();
    
    std::unique_ptr<AbstractSaveHandler> handler;
    Util::safeReplaceExtension(target, "xoppj");
    handler = std::make_unique<SqliteSaveHandler>();

    /*
        xoj::FileFormat format = xoj::getFormatFromPath(target);
        if (format == xoj::FileFormat::SQLITE) {
            Util::safeReplaceExtension(target, "xoppj");
            handler = std::make_unique<SqliteSaveHandler>();
        } else {
            Util::safeReplaceExtension(target, "xopp");
            handler = std::make_unique<SaveHandler>();
        }
    */

    doc->lock();
    handler->prepareSave(doc, target);
    doc->unlock();

    auto const createBackup = doc->shouldCreateBackupOnSave();
    if (createBackup) {
        try {
            Util::safeRenameFile(target, fs::path{target} += "~");
        } catch (const fs::filesystem_error& fe) {
            this->lastError = FS(_F("Save file error, can't backup: {1}") % std::string(fe.what()));
            return false;
        }
    }

    doc->lock();
    handler->saveTo(target, this->control);
    doc->setFilepath(target);
    doc->unlock();

    if (!handler->getErrorMessage().empty()) {
        this->lastError = FS(_F("Save file error: {1}") % handler->getErrorMessage());
        return false;
    } else if (createBackup) {
        try {
            fs::remove(fs::path{target} += "~");
        } catch (const fs::filesystem_error& fe) {
            g_warning("Could not delete backup! Failed with %s", fe.what());
        }
    } else {
        doc->setCreateBackupOnSave(true);
    }

    return true;
}