var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');
ctx.width = 600;
ctx.height = 600;

function atualizar() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            var img = this.responseText.split("|");
            for (var i = 0; i < 144; i++) {
                var y = i / 12|0;
                var x = i;
                if (y % 2 == 1) {
                    ctx.fillStyle = img[i];
                    ctx.fillRect((12 + (12*y) - x -1) * 50, y * 50, 50, 50);
                    ctx.strokeRect((12 + (12*y) - x -1) * 50, y * 50, 50, 50)
                } else {
                    ctx.fillStyle = img[i];
                    ctx.fillRect((x - (12*y)) * 50, y * 50, 50, 50);
                    ctx.strokeRect((x -(12*y))* 50, y * 50, 50, 50)
                }
            }
        }
    };
    xhttp.open("GET", "/show", true);
    xhttp.send();
};

setInterval(atualizar,100);