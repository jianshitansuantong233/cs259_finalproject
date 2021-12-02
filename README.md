# BFS Accelerator
UCLA CS 259 final project.

## Dependencies
- [TAPA](https://github.com/Blaok/tapa)

## Getting Started
### Setting up EC2 Instance
Use the `Vitis 2020.02` image.

Install TAPA with
```bash
sudo apt install ca-certificates
```
```bash
curl -L git.io/JnERa | bash
```
Certificates are reinstalled due to a faulty base image.

Setup TAPA environment
```bash
source <(curl -L bit.ly/3BzVG16)
```

The setup command must be run at startup. To add this to the `.bashrc` file, 
run
```bash
echo "source <(curl -L bit.ly/3BzVG16)" >> ~/.bashrc
```

### Building and Running the Code
Clone repo.
```bash
git clone git@github.com:jianshitansuantong233/cs259_finalproject.git
```

Build code.
```bash
mkdir build && cd build && cmake ..
```

Run software simulation (from `build` folder).
```bash
make swsim
```

Run hardware simulation (from `build` folder).
```bash
make hwsim
```
