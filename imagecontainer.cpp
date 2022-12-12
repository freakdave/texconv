#include <QDebug>
#include "imagecontainer.h"
#include "common.h"

bool ImageContainer::load(const QStringList& filenames, const int textureType, const int mipmapFilter, const bool mirrorVertically, const bool mirrorHorizontally) {
	const bool mipmapped	= (textureType & FLAG_MIPMAPPED);

	if ((filenames.size() > 1) && !mipmapped) {
		qCritical() << "Only one input file may be specified if no mipmap flag has been given.";
		return false;
	}

	// Load all given images
	foreach (const QString& filename, filenames) {
		const QImage img(filename);

		if (img.isNull()) {
			qCritical() << "Failed to load image" << filename;
			return false;
		}

		if (!isValidSize(img.width(), img.height(), textureType)) {
			qCritical("Image %s has an invalid texture size %dx%d", qPrintable(filename), img.width(), img.height());
			return false;
		}

		if (mipmapped && (img.width() != img.height())) {
			qCritical() << "Image" << filename << "is not square. Input images for mipmapped textures must be square";
			return false;
		}

        textureSize = textureSize.expandedTo(img.size());
        images.insert(img.width(), img.mirrored(mirrorHorizontally, mirrorVertically));

		qDebug() << "Loaded image" << filename;
	}


	if (mipmapped) {
        if (mipmapFilter == MIPMAP_FILTER_NEAREST) {
			qDebug("Using nearest-neighbor filtering for mipmaps");
        } else if (mipmapFilter == MIPMAP_FILTER_BILINEAR) {
			qDebug("Using bilinear filtering for mipmaps");
        } else if (mipmapFilter == MIPMAP_FILTER_KAISER) {
            qDebug("Using kaiser filtering for mipmaps");
        }

		// Generate any missing images by scaling down the size above them
		for (int size=(TEXTURE_SIZE_MAX/2); size>=1; size/=2) {
            if (images.contains(size*2) && !images.contains(size)) {
                const QImage mipmap = applyMipmapFilter(images.value(size * 2), size, mipmapFilter);

				images.insert(size, mipmap);
				qDebug("Generated %dx%d mipmap", size, size);
			}
		}
	}

	// Make sure we have at least one ok image
	if (width() < TEXTURE_SIZE_MIN || height() < TEXTURE_SIZE_MIN) {
		qCritical("At least one input image must be 8x8 or larger.");
		return false;
	}

	// Save keys for easy iteration
	keys = images.keys();
	std::sort(keys.begin(), keys.end());

	return true;
}


QImage ImageContainer::applyMipmapFilter(const QImage &source, int size, int mipmapFilter) {
    QImage mipmap = QImage(source.width(), source.height(), source.format());

    switch (mipmapFilter) {
        case MIPMAP_FILTER_NEAREST:
        case MIPMAP_FILTER_BILINEAR:
            mipmap = source.scaledToWidth(size, Qt::TransformationMode(mipmapFilter));
            break;

        case MIPMAP_FILTER_KAISER:
            mipmap = applyFilterKaiser(source, size);
            break;

        default:
            mipmap = source.scaledToWidth(size, Qt::TransformationMode(MIPMAP_FILTER_BILINEAR));
            break;
    }

    return mipmap;
}


QImage ImageContainer::applyFilterKaiser(const QImage &source, int size) {

        QImage image(size, size, source.format());
        QImage alpha = source.alphaChannel();

        // Iterate over the pixels in the new image
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                // Compute the corresponding pixel coordinates in the input image
                int oldX = 2 * x;
                int oldY = 2 * y;

                // Compute the weights for the Kaiser filters
                double w1 = windowKaiserBessel((double) x / (double)size - 0.5);
                double w2 = windowKaiserBessel((double) y / (double)size - 0.5);

                // Sample the alpha channel and pixel color
                int a = qAlpha(source.pixel(oldX, oldY));
                QRgb pixel = source.pixel(oldX, oldY);
                QColor oldColor(pixel);

                if(a > 0) {
                    // Apply the filter by combining the weighted values of the corresponding pixels
                    double opacity = (a / 255.0);

                    double r = w1 * w2 * oldColor.redF();
                    double g = w1 * w2 * oldColor.greenF();
                    double b = w1 * w2 * oldColor.blueF();

                    image.setPixel(x, y, qRgb((int)(r * 255.0), (int)(g * 255.0), (int)(b * 255.0)));

                    alpha.setPixel(x, y, (int)(opacity * 255.0));
                }
            }
        }

        image.setAlphaChannel(alpha);

        return image;
}


void ImageContainer::unloadAll() {
	textureSize = QSize(0, 0);
	images.clear();
	keys.clear();
}

QImage ImageContainer::getByIndex(int index, bool ascending) const {
	if (index >= keys.size()) {
		return QImage();
	} else {
		index = ascending ? index : (keys.size() - index - 1);
		int size = keys[index];
		return images.value(size);
	}
}


