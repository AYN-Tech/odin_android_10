$(warning "including ayn packages")
PRODUCT_PACKAGES += pservice
PRODUCT_PACKAGES += GameAssistant
PRODUCT_PACKAGES += OdinSettings
PRODUCT_PACKAGES += TouchMapping
#PRODUCT_PACKAGES += GamepadTest
PRODUCT_PACKAGES += FactoryTest
#PRODUCT_PACKAGES += FPS_2D
PRODUCT_PACKAGES += Gboard
PRODUCT_PACKAGES += Chrome
PRODUCT_PACKAGES += OdinLauncher
#PRODUCT_PACKAGES += Upgrade
PRODUCT_PACKAGES += WallpaperPicker3
PRODUCT_PACKAGES += ScreenCapture
PRODUCT_PACKAGES += SetupWizard

CURRENT_BUILD=M0
$(warning "=============PRODUCT.mk CURRENT_BUILD=$(CURRENT_BUILD)=============")
ifeq ($(CURRENT_BUILD),M0)

endif

PRODUCT_PACKAGES += Q4Ota
PRODUCT_PACKAGES += OdinDumpLog
