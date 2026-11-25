# Maintainer: Ethan Bautista
pkgname=fbi-dm-git
pkgver=r0
pkgrel=1
pkgdesc="Framebuffer-based login display manager"
arch=('x86_64')
url="https://git.ethanbtst.ca/fbi-dm"
license=('unknown')
depends=('pam' 'systemd')
makedepends=('gcc' 'git' 'make')
provides=("${pkgname%-git}")
conflicts=("${pkgname%-git}")
backup=('opt/fbi-dm/config.ini')
source=("${pkgname%-git}::git+https://git.ethanbtst.ca/fbi-dm")
sha256sums=('SKIP')

pkgver() {
	cd "$srcdir/${pkgname%-git}"
	printf 'r%s.%s\n' "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
	cd "$srcdir/${pkgname%-git}"
	make CC="${CC:-gcc}" \
		CFLAGS="${CFLAGS} -Wall -Wextra -Iinclude -pthread -O3 -march=native -flto -mtune=native -ffast-math" \
		LDFLAGS="${LDFLAGS} -pthread"
}

package() {
	cd "$srcdir/${pkgname%-git}"

	install -Dm755 fbi-dm "$pkgdir/opt/fbi-dm/fbi-dm"
	install -Dm644 res/config.ini "$pkgdir/opt/fbi-dm/config.ini"
	install -Dm644 res/fbi-dm@.service \
		"$pkgdir/usr/lib/systemd/system/fbi-dm@.service"
	install -dm755 "$pkgdir/opt/fbi-dm/img"
	cp -a img/. "$pkgdir/opt/fbi-dm/img/"
}
