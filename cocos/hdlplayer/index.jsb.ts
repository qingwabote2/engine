declare const jsb: any;

let prototype = (jsb.HDLPlayer as Function).prototype;

prototype.getTexImageSource = function(): TexImageSource | null {
    const info = this.takeImageInfo();
    if (!info) return null;

    if (!this._image) this._image = new Image()

    this._image.width = this._image.naturalWidth = info.width;
    this._image.height = this._image.naturalHeight = info.height;
    this._image._data = info.data;
    
    return this._image;
}

export const HDLPlayer = jsb.HDLPlayer;