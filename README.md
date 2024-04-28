# libreshop-client

Wii Homebrew downloader. Powered by Open Shop Channel

### Build instructions
Before building, install these dependencies:
```
pacman -S ppc-zip ppc-jansson wii-winyl ppc-yuarel
```
You will need to add the [nez-wii](https://wii.nezbednik.eu.org) pacman repository.

```
./data/create_acknowledgements
make
cp libreshop-client.dol libreshop/boot.dol
cp data/default_config.json libreshop/config.json
zip -r libreshop.zip libreshop/
```
